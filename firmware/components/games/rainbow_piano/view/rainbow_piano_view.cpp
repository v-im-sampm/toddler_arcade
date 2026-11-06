#include "little_arcade/games/rainbow_piano_view.hpp"

#include "little_arcade/games/rainbow_piano_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::rainbow_piano {

namespace {
using ui::to_lv;
namespace pal = ui::palette;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* src) {
  lv_obj_t* o = lv_image_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(o, src);
  return o;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}
}  // namespace

void RainbowPianoView::create(lv_obj_t* parent) {
  for (std::size_t i = 0; i < kKeyCount; ++i) {
    if (key_icons_[i].allocate(art::kIconSize, art::kIconSize)) {
      ui::Canvas c = key_icons_[i].canvas();
      art::paint_key_shape(c, i, pal::kWhite);
    }
  }
  for (std::size_t i = 0; i < kInstrumentCount; ++i) {
    if (instrument_icons_[i].allocate(art::kIconSize, art::kIconSize)) {
      ui::Canvas c = instrument_icons_[i].canvas();
      art::paint_instrument_icon(c, i);
    }
  }
  if (banner_.allocate(art::kBannerW, art::kBannerH)) {
    ui::Canvas c = banner_.canvas();
    art::paint_banner(c);
  }
  if (celebrate_icon_.allocate(art::kIconSize, art::kIconSize)) {
    ui::Canvas c = celebrate_icon_.canvas();
    art::paint_key_shape(c, 4, pal::kYellow);
  }

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::BRIDGE);
  if (banner_.ready()) {
    banner_obj_ = image(parent, banner_.dsc());
    lv_obj_align(banner_obj_, LV_ALIGN_TOP_MID, 0, 6);
  }

  for (std::size_t i = 0; i < kInstrumentCount; ++i) {
    const Rect& r = RainbowPianoGame::kInstrumentButtons[i];
    lv_obj_t* o = plain(parent);
    lv_obj_set_pos(o, r.x + 4, r.y + 4);
    lv_obj_set_size(o, r.w - 8, r.h - 8);
    lv_obj_set_style_radius(o, 10, 0);
    lv_obj_set_style_bg_color(o, to_lv(pal::kWhite), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, to_lv(pal::kChromeEdge), 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_outline_color(o, to_lv(pal::kYellow), 0);
    lv_obj_set_style_outline_width(o, 0, 0);
    instruments_[i] = o;
    if (instrument_icons_[i].ready()) {
      lv_obj_t* icon = image(o, instrument_icons_[i].dsc());
      lv_obj_center(icon);
    }
  }

  for (std::size_t i = 0; i < kKeyCount; ++i) {
    const Rect& r = RainbowPianoGame::kKeys[i];
    lv_obj_t* o = plain(parent);
    lv_obj_set_pos(o, r.x + 2, r.y + 2);
    lv_obj_set_size(o, r.w - 4, r.h - 4);
    lv_obj_set_style_radius(o, 9, 0);
    lv_obj_set_style_bg_color(o, to_lv(art::kKeyColors[i]), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, to_lv(pal::kInk), 0);
    lv_obj_set_style_border_width(o, 2, 0);
    // Warm, high-contrast key flash is the touch/sound feedback; no rapid animation.
    lv_obj_set_style_outline_color(o, to_lv(pal::kYellow), 0);
    lv_obj_set_style_outline_width(o, 0, 0);
    keys_[i] = o;
    if (key_icons_[i].ready()) {
      key_icons_obj_[i] = image(o, key_icons_[i].dsc());
      lv_obj_align(key_icons_obj_[i], LV_ALIGN_TOP_MID, 0, 42);
    }

    lv_obj_t* marker = plain(o);
    lv_obj_set_size(marker, 14, 14);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(marker, to_lv(pal::kWhite), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(marker, to_lv(pal::kInk), 0);
    lv_obj_set_style_border_width(marker, 2, 0);
    lv_obj_align(marker, LV_ALIGN_BOTTOM_MID, 0, -9);
    follow_markers_[i] = marker;
    set_visible(marker, false);
  }
  static constexpr int kStarX[] = {82, 146, 210};
  static constexpr int kStarY[] = {19, 8, 19};
  for (std::size_t i = 0; i < 3; ++i) {
    if (!celebrate_icon_.ready()) break;
    celebrate_stars_[i] = image(parent, celebrate_icon_.dsc());
    lv_obj_set_pos(celebrate_stars_[i], kStarX[i], kStarY[i]);
    set_visible(celebrate_stars_[i], false);
  }
  initialised_ = false;
}

void RainbowPianoView::sync(const RainbowPianoGame& game) {
  if (!initialised_) {
    shown_follow_ = -2;
    shown_instrument_ = -1;
    shown_phase_ = -1;
    for (std::size_t i = 0; i < kKeyCount; ++i) shown_key_lit_[i] = !game.key_lit(i);
    initialised_ = true;
  }

  const int phase = static_cast<int>(game.phase());
  if (phase != shown_phase_) {
    shown_phase_ = phase;
    const bool celebrating = game.phase() == Phase::CELEBRATE;
    for (lv_obj_t* star : celebrate_stars_) {
      if (star) set_visible(star, celebrating);
    }
  }

  const int selected = static_cast<int>(game.instrument());
  if (selected != shown_instrument_) {
    shown_instrument_ = selected;
    for (std::size_t i = 0; i < kInstrumentCount; ++i) {
      const bool active = static_cast<int>(i) == selected;
      lv_obj_set_style_bg_color(instruments_[i], to_lv(active ? 0xFFF4C2 : pal::kWhite), 0);
      lv_obj_set_style_border_color(instruments_[i], to_lv(active ? pal::kOrange : pal::kChromeEdge), 0);
      lv_obj_set_style_border_width(instruments_[i], active ? 4 : 2, 0);
    }
  }

  const int follow = game.follow_key();
  if (follow != shown_follow_) {
    shown_follow_ = follow;
    for (std::size_t i = 0; i < kKeyCount; ++i) set_visible(follow_markers_[i], follow == static_cast<int>(i));
  }

  for (std::size_t i = 0; i < kKeyCount; ++i) {
    const bool lit = game.key_lit(i);
    if (lit == shown_key_lit_[i]) continue;
    shown_key_lit_[i] = lit;
    lv_obj_set_style_outline_width(keys_[i], lit ? 4 : 0, 0);
    lv_obj_set_style_border_color(keys_[i], to_lv(lit ? pal::kYellow : pal::kInk), 0);
  }
}

}  // namespace little_arcade::rainbow_piano
