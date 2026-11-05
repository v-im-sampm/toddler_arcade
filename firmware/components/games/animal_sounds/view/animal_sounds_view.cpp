#include "little_arcade/games/animal_sounds_view.hpp"

#include <cmath>

#include "little_arcade/games/animal_sounds_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::animal_sounds {
namespace {
using ui::to_lv;
namespace pal = ui::palette;
using G = AnimalSoundsGame;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return obj;
}

lv_obj_t* band(lv_obj_t* parent, int y, int h, uint32_t rgb) {
  lv_obj_t* obj = plain(parent);
  lv_obj_set_pos(obj, 0, y);
  lv_obj_set_size(obj, kScreenWidth, h);
  lv_obj_set_style_bg_color(obj, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  return obj;
}

void set_flag_visible(lv_obj_t* obj, bool visible) {
  if (visible) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t* image(lv_obj_t* parent) {
  lv_obj_t* obj = lv_image_create(parent);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}
}  // namespace

void AnimalSoundsView::set_visible(lv_obj_t* obj, bool visible) { set_flag_visible(obj, visible); }

void AnimalSoundsView::create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  band(parent, 0, 62, pal::kSkyDark);
  band(parent, kScreenHeight - 10, 10, pal::kGround);

  for (std::size_t i = 0; i < kAnimalCount; ++i) {
    if (animals_[i].allocate(art::kAnimalW, art::kAnimalH)) {
      ui::Canvas c = animals_[i].canvas();
      art::paint_animal(c, static_cast<Animal>(i));
    }
    if (clues_[i].allocate(art::kClueSize, art::kClueSize)) {
      ui::Canvas c = clues_[i].canvas();
      art::paint_clue(c, clue_for(static_cast<Animal>(i)));
    }
  }

  for (CardNode& node : cards_) {
    node.shadow = plain(parent);
    lv_obj_set_style_radius(node.shadow, 12, 0);
    lv_obj_set_style_bg_color(node.shadow, to_lv(pal::kInk), 0);
    lv_obj_set_style_bg_opa(node.shadow, LV_OPA_20, 0);

    node.card = plain(parent);
    lv_obj_set_style_radius(node.card, 10, 0);
    lv_obj_set_style_bg_color(node.card, to_lv(pal::kWhite), 0);
    lv_obj_set_style_bg_opa(node.card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(node.card, to_lv(pal::kChromeEdge), 0);
    lv_obj_set_style_border_width(node.card, 2, 0);
    lv_obj_set_style_clip_corner(node.card, true, 0);

    node.image = image(node.card);
    lv_obj_center(node.image);
    node.visible = true;
    set_visible(node.shadow, false);
    set_visible(node.card, false);
    node.visible = false;
  }

  const Rect& r = G::kSoundButton;
  speaker_ = plain(parent);
  lv_obj_set_pos(speaker_, r.x, r.y);
  lv_obj_set_size(speaker_, r.w, r.h);
  lv_obj_set_style_radius(speaker_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(speaker_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_bg_opa(speaker_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(speaker_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(speaker_, 2, 0);
  lv_obj_set_style_outline_color(speaker_, to_lv(pal::kYellow), 0);
  lv_obj_set_style_outline_width(speaker_, 0, 0);
  lv_obj_set_style_outline_pad(speaker_, 0, 0);

  speaker_icon_ = lv_label_create(parent);
  lv_label_set_text_static(speaker_icon_, LV_SYMBOL_VOLUME_MAX);
  lv_obj_set_style_text_color(speaker_icon_, to_lv(pal::kBlue), 0);
  lv_obj_set_pos(speaker_icon_, r.x + 8, r.y + 8);

  question_ = lv_label_create(parent);
  lv_label_set_text_static(question_, "?");
#if LV_FONT_MONTSERRAT_28
  lv_obj_set_style_text_font(question_, &lv_font_montserrat_28, 0);
#endif
  lv_obj_set_style_text_color(question_, to_lv(pal::kInk), 0);
  lv_obj_set_pos(question_, r.x + 34, r.y + 25);

  clue_icon_ = image(parent);
  lv_obj_set_size(clue_icon_, art::kClueSize, art::kClueSize);
  lv_obj_set_pos(clue_icon_, r.x + r.w - art::kClueSize - 1, r.y + r.h - art::kClueSize - 1);
  set_visible(clue_icon_, false);
  initialised_ = false;
}

void AnimalSoundsView::sync_card(CardNode& node, const Card& card, int index, int hint_index,
                                int celebrating_index, int wiggling_index, uint32_t now_ms) {
  if (!node.visible) {
    node.visible = true;
    set_visible(node.shadow, true);
    set_visible(node.card, true);
  }
  if (node.animal != static_cast<int>(card.animal)) {
    node.animal = static_cast<int>(card.animal);
    const ui::ArgbImage& art_image = animals_[static_cast<std::size_t>(node.animal)];
    if (art_image.ready()) lv_image_set_src(node.image, art_image.dsc());
  }

  const bool hinted = index == hint_index;
  const bool celebrating = index == celebrating_index;
  const int style = hinted ? 1 : (celebrating ? 2 : 0);
  if (style != node.style) {
    node.style = style;
    lv_obj_set_style_bg_color(node.card, to_lv(hinted ? 0xFFF0A8 : (celebrating ? 0xE2F7CF : pal::kWhite)), 0);
    lv_obj_set_style_border_color(node.card, to_lv(hinted ? pal::kYellow : (celebrating ? pal::kGreen : pal::kChromeEdge)), 0);
    lv_obj_set_style_border_width(node.card, hinted || celebrating ? 4 : 2, 0);
    lv_obj_set_style_outline_color(node.card, to_lv(pal::kWhite), 0);
    lv_obj_set_style_outline_width(node.card, hinted ? 2 : 0, 0);
  }

  float wiggle = 0.0f;
  if (wiggling_index == index) {
    const uint32_t elapsed = now_ms - wiggle_start_ms_;
    wiggle = std::sin(static_cast<float>(elapsed) / 40.0f) * 4.0f;
  }
  float bounce = 0.0f;
  if (celebrating) {
    const float elapsed = static_cast<float>(now_ms - phase_start_ms_);
    bounce = -std::fabs(std::sin(elapsed / 110.0f)) * 4.0f;
  }
  const int16_t px = static_cast<int16_t>(card.rect.x + 4 + std::lround(wiggle));
  const int16_t py = static_cast<int16_t>(card.rect.y + 4 + std::lround(bounce));
  if (node.x != px || node.y != py) {
    node.x = px;
    node.y = py;
    lv_obj_set_pos(node.shadow, px + 1, py + 2);
    lv_obj_set_pos(node.card, px, py);
    lv_obj_set_size(node.shadow, card.rect.w - 6, card.rect.h - 6);
    lv_obj_set_size(node.card, card.rect.w - 8, card.rect.h - 8);
  }
}

void AnimalSoundsView::sync(const AnimalSoundsGame& game, uint32_t now_ms) {
  if (!initialised_ || game.phase() != last_phase_) {
    last_phase_ = game.phase();
    phase_start_ms_ = now_ms;
    initialised_ = true;
  }
  if (game.wiggling() >= 0 && game.wiggling() != wiggle_index_) {
    wiggle_index_ = game.wiggling();
    wiggle_start_ms_ = now_ms;
  } else if (game.wiggling() < 0) {
    wiggle_index_ = -1;
  }

  const int hint_index = game.hint_active() ? game.target_index() : -1;
  const int celebrating_index = game.celebrating();
  for (std::size_t i = 0; i < G::kMaxCards; ++i) {
    CardNode& node = cards_[i];
    if (i < game.card_count()) {
      sync_card(node, game.card(i), static_cast<int>(i), hint_index, celebrating_index, game.wiggling(), now_ms);
    } else if (node.visible) {
      node.visible = false;
      set_visible(node.shadow, false);
      set_visible(node.card, false);
    }
  }

  const bool clue_visible = game.sound_playing() || game.hint_active();
  const std::size_t animal = static_cast<std::size_t>(game.target());
  set_visible(question_, !clue_visible && game.phase() == Phase::INTRO);
  set_visible(speaker_icon_, !game.sound_playing());
  set_visible(clue_icon_, clue_visible);
  if (clue_visible && clues_[animal].ready()) lv_image_set_src(clue_icon_, clues_[animal].dsc());

  const int sound_state = game.sound_playing() ? 1 : (game.hint_active() ? 2 : 0);
  if (sound_state != sound_state_) {
    sound_state_ = sound_state;
    lv_obj_set_style_bg_color(speaker_, to_lv(sound_state ? 0xFFF3BA : pal::kWhite), 0);
    lv_obj_set_style_outline_width(speaker_, sound_state ? 3 : 0, 0);
    lv_obj_set_style_outline_pad(speaker_, sound_state ? 2 : 0, 0);
  }
}

}  // namespace little_arcade::animal_sounds
