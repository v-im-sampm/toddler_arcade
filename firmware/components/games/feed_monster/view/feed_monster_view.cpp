#include "little_arcade/games/feed_monster_view.hpp"

#include <cmath>

#include "little_arcade/games/feed_monster_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::feed_monster {

namespace {
using ui::to_lv;
namespace pal = ui::palette;
using G = FeedMonsterGame;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t* disc(lv_obj_t* parent, int d, uint32_t rgb, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_size(o, d, d);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  return o;
}

lv_obj_t* snack_icon(lv_obj_t* parent, int size) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_size(o, size, size);
  lv_obj_t* fruit = disc(o, size - 5, pal::kOrange);
  lv_obj_set_pos(fruit, 0, 5);
  lv_obj_set_style_border_color(fruit, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(fruit, 1, 0);
  lv_obj_t* leaf = disc(o, size / 3, pal::kGreen);
  lv_obj_set_pos(leaf, size / 2, 0);
  lv_obj_set_style_border_color(leaf, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(leaf, 1, 0);
  return o;
}

lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* src) {
  lv_obj_t* o = lv_image_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  if (src) lv_image_set_src(o, src);
  return o;
}

lv_obj_t* card(lv_obj_t* parent, const Rect& r, uint32_t rgb) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, r.x + 3, r.y + 3);
  lv_obj_set_size(o, r.w - 6, r.h - 6);
  lv_obj_set_style_radius(o, 12, 0);
  lv_obj_set_style_bg_color(o, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, to_lv(pal::kChromeEdge), 0);
  lv_obj_set_style_border_width(o, 2, 0);
  return o;
}

void visible(lv_obj_t* obj, bool show) {
  const bool hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (hidden == !show) return;
  if (show) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void band(lv_obj_t* parent, int y, int h, uint32_t rgb) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, 0, y);
  lv_obj_set_size(o, kScreenWidth, h);
  lv_obj_set_style_bg_color(o, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

}  // namespace

void FeedMonsterView::create(lv_obj_t* parent) {
  if (monster_img_.allocate(art::kMonsterW, art::kMonsterH)) {
    ui::Canvas c = monster_img_.canvas();
    art::paint_monster_image(c);
  }
  if (bowl_img_.allocate(art::kBowlW, art::kBowlH)) {
    ui::Canvas c = bowl_img_.canvas();
    art::paint_bowl_image(c);
  }
  if (snack_img_.allocate(art::kSnackW, art::kSnackH)) {
    ui::Canvas c = snack_img_.canvas();
    art::paint_snack_image(c);
  }

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  band(parent, 0, 62, pal::kSkyDark);
  band(parent, kScreenHeight - 10, 10, pal::kGround);

  badge_ = card(parent, G::kRequestBadge, pal::kWhite);
  for (int i = 0; i < G::kMaxRequest; ++i) {
    request_snacks_[i] = snack_icon(badge_, 17);
  }

  bowl_ = image(parent, bowl_img_.ready() ? bowl_img_.dsc() : nullptr);
  lv_obj_set_pos(bowl_, G::kFoodBowl.x + 2, G::kFoodBowl.y + 1);
  monster_ = image(parent, monster_img_.ready() ? monster_img_.dsc() : nullptr);
  lv_image_set_pivot(monster_, art::kMonsterW / 2, art::kMonsterH / 2);
  lv_obj_set_pos(monster_, 152, 58);

  for (int i = 0; i < G::kMaxRequest; ++i) {
    tummy_dots_[i] = disc(parent, 10, pal::kWhite);
    lv_obj_set_pos(tummy_dots_[i], 224 - 16 + i * 16, 153);
    lv_obj_set_style_border_color(tummy_dots_[i], to_lv(pal::kInk), 0);
    lv_obj_set_style_border_width(tummy_dots_[i], 1, 0);
  }

  for (std::size_t n = 0; n < G::kNumberButtonCount; ++n) {
    const Rect& r = G::kNumberButtons[n];
    number_cards_[n] = card(parent, r, pal::kWhite);
    for (std::size_t dot_i = 0; dot_i <= n; ++dot_i) {
      number_snacks_[n][dot_i] = snack_icon(number_cards_[n], 17);
      const int x = (r.w - static_cast<int>(n + 1) * 22) / 2 + static_cast<int>(dot_i) * 22;
      lv_obj_set_pos(number_snacks_[n][dot_i], x, 16);
    }
  }

  hint_ring_ = plain(parent);
  lv_obj_set_pos(hint_ring_, 160, 63);
  lv_obj_set_size(hint_ring_, 128, 112);
  lv_obj_set_style_radius(hint_ring_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(hint_ring_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(hint_ring_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_border_width(hint_ring_, 4, 0);

  snack_ = image(parent, snack_img_.ready() ? snack_img_.dsc() : nullptr);
  lv_obj_set_size(snack_, art::kSnackW, art::kSnackH);
  lv_image_set_pivot(snack_, art::kSnackW / 2, art::kSnackH / 2);

  static constexpr uint32_t kSparkleColors[kSparkles] = {
      pal::kYellow, pal::kPink, pal::kBlue, pal::kWhite, pal::kOrange, pal::kGreen, pal::kPurple};
  static constexpr int kSparkleX[kSparkles] = {166, 192, 230, 268, 281, 242, 180};
  static constexpr int kSparkleY[kSparkles] = {78, 62, 70, 88, 125, 157, 145};
  for (int i = 0; i < kSparkles; ++i) {
    sparkles_[i] = disc(parent, i % 2 ? 7 : 9, kSparkleColors[i]);
    lv_obj_set_pos(sparkles_[i], kSparkleX[i], kSparkleY[i]);
  }

  visible(hint_ring_, false);
  visible(snack_, false);
  for (lv_obj_t* snack : request_snacks_) visible(snack, false);
  for (lv_obj_t* sparkle : sparkles_) visible(sparkle, false);
  initialised_ = false;
}

void FeedMonsterView::sync(const FeedMonsterGame& game, uint32_t now_ms) {
  const Phase phase = game.phase();
  if (!initialised_ || phase != last_phase_) {
    last_phase_ = phase;
    phase_start_ms_ = now_ms;
    if (!initialised_) last_fed_ = game.fed();
  }
  initialised_ = true;

  if (game.requested() != shown_request_) {
    shown_request_ = game.requested();
    const int first = (G::kRequestBadge.w - static_cast<int>(shown_request_) * 22) / 2;
    for (int i = 0; i < G::kMaxRequest; ++i) {
      visible(request_snacks_[i], i < shown_request_);
      visible(tummy_dots_[i], i < shown_request_);
      lv_obj_set_pos(request_snacks_[i], first + i * 22, 16);
    }
    for (std::size_t n = 0; n < G::kNumberButtonCount; ++n) {
      lv_obj_set_style_bg_color(number_cards_[n],
                                to_lv(n + 1 == game.requested() ? pal::kShell : pal::kWhite), 0);
    }
  }
  if (game.fed() != shown_fed_) {
    shown_fed_ = game.fed();
    for (int i = 0; i < G::kMaxRequest; ++i) {
      lv_obj_set_style_bg_color(tummy_dots_[i], to_lv(i < shown_fed_ ? pal::kOrange : pal::kWhite), 0);
    }
  }

  if (game.fed() != last_fed_) {
    if (game.fed() > last_fed_ && phase == Phase::PLAY) {
      flight_start_ms_ = now_ms;
      flight_active_ = true;
    }
    last_fed_ = game.fed();
  }

  const uint32_t elapsed = now_ms - phase_start_ms_;
  int32_t monster_y = 58;
  if (phase == Phase::CELEBRATE) {
    const float t = static_cast<float>(elapsed) / static_cast<float>(G::kCelebrateMs);
    monster_y -= static_cast<int32_t>(14.0f * std::sin(3.1415926f * (t > 1.0f ? 1.0f : t)));
  }
  if (game.giggling()) {
    const float wiggle = std::sin(static_cast<float>(now_ms % 500u) * 0.01256637f);
    const int32_t rotation = static_cast<int32_t>(std::lround(wiggle * 35.0f));
    if (rotation != shown_rotation_) {
      shown_rotation_ = rotation;
      lv_image_set_rotation(monster_, rotation);
    }
  } else {
    if (shown_rotation_ != 0) {
      shown_rotation_ = 0;
      lv_image_set_rotation(monster_, 0);
    }
  }
  if (monster_y != shown_monster_y_) {
    shown_monster_y_ = monster_y;
    lv_obj_set_pos(monster_, 152, monster_y);
  }

  const bool hint = game.hint_active();
  visible(hint_ring_, hint);
  if (hint) {
    const uint8_t opa = static_cast<uint8_t>(170 + 70 * std::sin(static_cast<float>(now_ms % 1200u) * 0.00523599f));
    lv_obj_set_style_border_opa(hint_ring_, static_cast<lv_opa_t>(opa), 0);
  }
  for (lv_obj_t* sparkle : sparkles_) visible(sparkle, phase == Phase::CELEBRATE);

  if (flight_active_) {
    const uint32_t flight_ms = now_ms - flight_start_ms_;
    if (flight_ms >= G::kChewMs || phase != Phase::PLAY) {
      flight_active_ = false;
      visible(snack_, false);
    } else {
      const float t = static_cast<float>(flight_ms) / static_cast<float>(G::kChewMs);
      const int32_t x = static_cast<int32_t>(std::lround(62.0f + (218.0f - 62.0f) * t));
      const int32_t y = static_cast<int32_t>(std::lround(121.0f + 32.0f * t - 18.0f * std::sin(3.1415926f * t)));
      lv_obj_set_pos(snack_, x - art::kSnackW / 2, y - art::kSnackH / 2);
      visible(snack_, true);
    }
  } else {
    visible(snack_, false);
  }
}

}  // namespace little_arcade::feed_monster
