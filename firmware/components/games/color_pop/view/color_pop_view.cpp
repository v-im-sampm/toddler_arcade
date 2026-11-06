#include "little_arcade/games/color_pop_view.hpp"

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "little_arcade/ui/scene_art.hpp"
#include "little_arcade/ui/shade.hpp"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

namespace little_arcade::color_pop {
namespace {
constexpr float kPi = 3.14159265f;
constexpr int kBubbleSize = 72;
constexpr int kBadgeSize = 56;
constexpr int kCrabW = 78;
constexpr int kCrabH = 56;
constexpr int kHandW = 24;
constexpr int kHandH = 30;

const ui::tok::Hue& hue_for(Color color) {
  switch (color) {
    case Color::RED: return ui::tok::kRed;
    case Color::BLUE: return ui::tok::kBlue;
    case Color::GREEN: return ui::tok::kGreen;
    case Color::YELLOW: return ui::tok::kYellow;
  }
  return ui::tok::kBlue;
}

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return obj;
}

void visible(lv_obj_t* obj, bool on) {
  if (on) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void report_art_oom(const char* name) {
#ifdef ESP_PLATFORM
  ESP_LOGE("color_pop_view", "Unable to allocate %s art", name);
#else
  std::fprintf(stderr, "color_pop_view: unable to allocate %s art\n", name);
#endif
}
}  // namespace

void ColorPopView::paint_pattern(ui::Canvas& c, Pattern pattern, float cx, float cy, float size, uint32_t color) {
  switch (pattern) {
    case Pattern::HEART:
      ui::paint(c, cx - size, cy - size, cx + size, cy + size, color, 242,
                [=](float x, float y) { return ui::in_heart(x, y, cx, cy, size * 1.9f); });
      break;
    case Pattern::WAVE:
      for (int line = 0; line < 2; ++line) {
        ui::Path p;
        const float y0 = cy + (static_cast<float>(line) - 0.5f) * size * 0.55f;
        for (int i = 0; i <= 12; ++i) {
          const float x = cx - size * 0.82f + static_cast<float>(i) * size * 1.64f / 12.0f;
          const float y = y0 + std::sin(static_cast<float>(i) * kPi / 3.0f) * size * 0.13f;
          if (i == 0) p.move(x, y);
          else p.line(x, y);
        }
        ui::stroke_path(c, p, 2.4f, color);
      }
      break;
    case Pattern::LEAF: {
      ui::Path leaf;
      leaf.move(cx - size * 0.68f, cy + size * 0.58f)
          .quad(cx - size * 0.8f, cy - size * 0.64f, cx + size * 0.73f, cy - size * 0.47f)
          .quad(cx + size * 0.76f, cy + size * 0.73f, cx - size * 0.68f, cy + size * 0.58f);
      ui::fill_path(c, leaf, color, 240);
      ui::stroke_path(c, ui::Path{}.move(cx - size * 0.6f, cy + size * 0.48f)
                                      .quad(cx - size * 0.05f, cy - size * 0.05f, cx + size * 0.55f, cy - size * 0.37f),
                      1.6f, ui::tok::kGreen.shadow, true, 180);
      break;
    }
    case Pattern::STAR:
      ui::fill_star(c, cx, cy, size, size * 0.46f, color);
      break;
  }
}

void ColorPopView::paint_bubble(ui::ArgbImage& image, Color color) {
  if (!image.allocate(kBubbleSize, kBubbleSize)) {
    report_art_oom("bubble");
    return;
  }
  ui::Canvas c = image.canvas();
  c.clear();
  const float cx = 36.0f, cy = 34.0f, r = 29.0f;
  ui::soft_shadow(c, cx + 2.0f, 66.0f, 23.0f, 4.5f, 38);
  ui::shaded_ball(c, cx, cy, r, hue_for(color), 2.0f);

  static constexpr uint32_t rim[] = {0xFF9ED9, 0xFFE58A, 0x9DE9D1, 0x9FC4FF};
  for (int i = 0; i < 4; ++i) {
    const float a0 = -0.5f + static_cast<float>(i) * 1.57f;
    ui::Path arc;
    arc.arc(cx, cy, r - 3.0f, a0, a0 + 1.12f, 12);
    ui::stroke_path(c, arc, 2.2f, rim[i], false, 185);
  }
  paint_pattern(c, pattern_for(color), cx, cy + 2.0f, 12.0f, 0xF5FFFFFF);
  ui::gloss(c, cx, cy, r, 205);
}

void ColorPopView::paint_badge(ui::ArgbImage& image, const ui::tok::Hue* hue, bool happy) {
  if (!image.allocate(kBadgeSize, kBadgeSize)) {
    report_art_oom("target badge");
    return;
  }
  ui::Canvas c = image.canvas();
  c.clear();
  ui::soft_shadow(c, 28.0f, 53.0f, 19.0f, 3.0f, 30);
  ui::shaded_ball(c, 28.0f, 27.0f, 24.5f, ui::tok::kCream, 2.0f);
  if (happy) {
    ui::cartoon_eye(c, 20.0f, 25.0f, 4.2f);
    ui::cartoon_eye(c, 36.0f, 25.0f, 4.2f);
    ui::cheeks(c, 28.0f, 34.0f, 15.0f, 3.0f);
    ui::smile(c, 28.0f, 32.0f, 9.0f, 2.3f);
  } else if (hue) {
    ui::shaded_ball(c, 28.0f, 27.0f, 17.0f, *hue, 1.2f);
    const std::size_t index = hue == &ui::tok::kRed    ? 0
                              : hue == &ui::tok::kBlue ? 1
                              : hue == &ui::tok::kGreen ? 2
                                                        : 3;
    paint_pattern(c, pattern_for(static_cast<Color>(index)), 28.0f, 28.0f, 7.0f, ui::tok::kWhite);
    ui::gloss(c, 28.0f, 27.0f, 17.0f, 200);
  }
}

void ColorPopView::paint_crab(ui::Canvas& c) {
  const float cx = 39.0f;
  for (int side : {-1, 1}) {
    for (int leg = 0; leg < 3; ++leg) {
      const float x0 = cx + static_cast<float>(side) * (12.0f + leg * 3.0f);
      const float x1 = cx + static_cast<float>(side) * (20.0f + leg * 5.0f);
      ui::stroke_path(c, ui::Path{}.move(x0, 40.0f).quad(x1, 45.0f, x1 + side * 2.0f, 50.0f),
                      2.2f, ui::tok::kOrange.shadow);
    }
    const float claw_x = cx + static_cast<float>(side) * 28.0f;
    const float claw_y = 20.0f;
    ui::stroke_path(c, ui::Path{}.move(cx + side * 15.0f, 27.0f).quad(claw_x, 27.0f, claw_x, claw_y + 5.0f),
                    3.0f, ui::tok::kOrange.shadow);
    ui::shaded_ball(c, claw_x, claw_y, 8.0f, ui::tok::kOrange, 1.4f);
    ui::fill_ellipse(c, claw_x - side * 1.7f, claw_y - 4.0f, 2.6f, 1.3f, -0.5f, ui::tok::kCream.light);
  }
  ui::shaded_ellipse(c, cx, 35.0f, 22.0f, 15.0f, ui::tok::kOrange, 2.0f);
  ui::stroke_path(c, ui::Path{}.move(cx - 6.0f, 24.0f).line(cx - 8.0f, 12.0f), 2.2f, ui::tok::kOrange.shadow);
  ui::stroke_path(c, ui::Path{}.move(cx + 6.0f, 24.0f).line(cx + 8.0f, 12.0f), 2.2f, ui::tok::kOrange.shadow);
  ui::cartoon_eye(c, cx - 8.0f, 12.0f, 5.2f);
  ui::cartoon_eye(c, cx + 8.0f, 12.0f, 5.2f);
  ui::cheeks(c, cx, 37.0f, 12.0f, 2.7f);
  ui::smile(c, cx, 34.0f, 6.0f, 1.8f);
}

void ColorPopView::create(lv_obj_t* parent) {
  ui::show_backdrop(parent, ui::World::BEACH);

  for (std::size_t i = 0; i < kColorCount; ++i) {
    paint_bubble(bubble_art_[i], static_cast<Color>(i));
  }
  for (std::size_t i = 0; i < kColorCount; ++i) paint_badge(badge_art_[i], &hue_for(static_cast<Color>(i)), false);
  paint_badge(badge_art_[kColorCount], nullptr, true);
  paint_badge(badge_art_[kColorCount + 1], nullptr, false);

  if (!crab_art_.allocate(kCrabW, kCrabH)) {
    report_art_oom("crab");
  } else {
    ui::Canvas c = crab_art_.canvas();
    c.clear();
    paint_crab(c);
  }
  crab_ = ui::sprite(parent, crab_art_);
  ui::place_sprite(crab_, crab_art_, 42.0f, 205.0f);

  const Rect& badge_rect = ColorPopGame::kTargetBadge;
  badge_ = lv_image_create(parent);
  lv_obj_remove_flag(badge_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(badge_, badge_rect.x, badge_rect.y);
  lv_image_set_src(badge_, badge_art_[kColorCount + 1].dsc());
  badge_question_ = lv_label_create(parent);
  lv_label_set_text_static(badge_question_, "?");
#if LV_FONT_MONTSERRAT_28
  lv_obj_set_style_text_font(badge_question_, &lv_font_montserrat_28, 0);
#endif
  lv_obj_set_style_text_color(badge_question_, lv_color_hex(ui::tok::kInk), 0);
  lv_obj_set_pos(badge_question_, badge_rect.x, badge_rect.y + 8);
  lv_obj_set_width(badge_question_, badge_rect.w);
  lv_obj_set_style_text_align(badge_question_, LV_TEXT_ALIGN_CENTER, 0);

  hint_ring_ = plain(parent);
  lv_obj_set_size(hint_ring_, 72, 72);
  lv_obj_set_style_radius(hint_ring_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(hint_ring_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(hint_ring_, lv_color_hex(ui::tok::kHintGlow), 0);
  lv_obj_set_style_border_width(hint_ring_, 3, 0);
  lv_obj_set_style_border_opa(hint_ring_, LV_OPA_70, 0);
  visible(hint_ring_, false);

  if (!hand_art_.allocate(kHandW, kHandH)) {
    report_art_oom("helper hand");
  } else {
    ui::Canvas c = hand_art_.canvas();
    c.clear();
    ui::shaded_round_rect(c, 8, 8, 17, 29, 5, ui::tok::kCream, 1.2f);
    ui::shaded_round_rect(c, 10, 1, 17, 18, 3.5f, ui::tok::kCream, 1.2f);
    ui::shaded_ellipse(c, 6, 18, 5, 6, ui::tok::kCream, 1.2f);
    ui::shaded_ellipse(c, 18, 22, 4, 5, ui::tok::kCream, 1.2f);
  }
  helper_hand_ = ui::sprite(parent, hand_art_);
  visible(helper_hand_, false);

  for (std::size_t i = 0; i < ColorPopGame::kMaxBubbles; ++i) {
    shadows_[i] = plain(parent);
    lv_obj_set_size(shadows_[i], 44, 10);
    lv_obj_set_style_radius(shadows_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shadows_[i], lv_color_hex(ui::tok::kInk), 0);
    lv_obj_set_style_bg_opa(shadows_[i], LV_OPA_20, 0);
    bubbles_[i] = ui::sprite(parent, bubble_art_[0]);
    visible(shadows_[i], false);
    visible(bubbles_[i], false);
    cache_[i] = BubbleCache{};
  }

  intro_play_ = plain(parent);
  lv_obj_set_pos(intro_play_, 107, 86);
  lv_obj_set_size(intro_play_, 106, 106);
  lv_obj_set_style_radius(intro_play_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(intro_play_, lv_color_hex(ui::tok::kCream.light), 0);
  lv_obj_set_style_bg_grad_color(intro_play_, lv_color_hex(ui::tok::kCream.base), 0);
  lv_obj_set_style_bg_grad_dir(intro_play_, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(intro_play_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(intro_play_, lv_color_hex(ui::tok::kCream.shadow), 0);
  lv_obj_set_style_border_width(intro_play_, 3, 0);
  lv_obj_t* play = lv_label_create(intro_play_);
  lv_label_set_text_static(play, LV_SYMBOL_PLAY);
#if LV_FONT_MONTSERRAT_40
  lv_obj_set_style_text_font(play, &lv_font_montserrat_40, 0);
#endif
  lv_obj_set_style_text_color(play, lv_color_hex(ui::tok::kGreen.base), 0);
  lv_obj_center(play);

  fx_.create(parent, 1234);
  last_phase_ = Phase::INTRO;
  last_round_ = 0;
  last_hint_ms_ = 0;
  last_wiggle_ = -1;
  badge_variant_ = -1;
  have_time_ = false;
  crab_spring_.snap(1.0f);
}

void ColorPopView::sync_badge(const ColorPopGame& game) {
  const int variant = game.phase() == Phase::CELEBRATE ? static_cast<int>(kColorCount)
                      : game.phase() == Phase::INTRO   ? static_cast<int>(kColorCount + 1)
                                                       : static_cast<int>(game.target());
  if (variant == badge_variant_) return;
  badge_variant_ = variant;
  lv_image_set_src(badge_, badge_art_[static_cast<std::size_t>(variant)].dsc());
  lv_obj_invalidate(badge_);
  visible(badge_question_, game.phase() == Phase::INTRO);
}

void ColorPopView::sync_bubbles(const ColorPopGame& game, uint32_t now_ms) {
  const int wiggle = game.wiggling();
  if (wiggle != last_wiggle_) {
    last_wiggle_ = wiggle;
    wiggle_start_ms_ = now_ms;
  }

  int target_index = -1;
  for (std::size_t i = 0; i < game.bubble_count(); ++i) {
    if (game.bubble(i).color == game.target()) target_index = static_cast<int>(i);
  }
  const bool hint = game.hint_active() && game.phase() == Phase::PLAY && target_index >= 0;
  visible(hint_ring_, hint);
  visible(helper_hand_, hint);

  for (std::size_t i = 0; i < ColorPopGame::kMaxBubbles; ++i) {
    const bool shown = i < game.bubble_count() && !game.bubble(i).popped;
    if (shown != cache_[i].visible) {
      cache_[i].visible = shown;
      visible(bubbles_[i], shown);
      visible(shadows_[i], shown);
    }
    if (!shown) continue;

    const Bubble& bubble = game.bubble(i);
    const int color = static_cast<int>(bubble.color);
    if (color != cache_[i].color) {
      cache_[i].color = color;
      lv_image_set_src(bubbles_[i], bubble_art_[color].dsc());
    }
    if (bubble.rect.x != cache_[i].x || bubble.rect.y != cache_[i].y) {
      cache_[i].x = bubble.rect.x;
      cache_[i].y = bubble.rect.y;
      const int x = bubble.rect.x + (bubble.rect.w - kBubbleSize) / 2;
      const int y = bubble.rect.y + (bubble.rect.h - kBubbleSize) / 2;
      lv_obj_set_pos(bubbles_[i], x, y);
      lv_obj_set_pos(shadows_[i], x + 14, y + 62);
    }

    const float breath = ui::motion::breath(now_ms, static_cast<float>(i) * 0.19f, 0.018f);
    const int sx = static_cast<int>(std::lround(256.0f * (1.0f + breath)));
    const int sy = static_cast<int>(std::lround(256.0f * (1.0f - breath)));
    int rotation = 0;
    if (static_cast<int>(i) == last_wiggle_) {
      const uint32_t elapsed = now_ms - wiggle_start_ms_;
      rotation = static_cast<int>(std::lround(ui::motion::wiggle_deg(elapsed) * 10.0f));
    }
    if (sx != cache_[i].scale_x) {
      cache_[i].scale_x = sx;
      lv_image_set_scale_x(bubbles_[i], static_cast<uint32_t>(sx));
    }
    if (sy != cache_[i].scale_y) {
      cache_[i].scale_y = sy;
      lv_image_set_scale_y(bubbles_[i], static_cast<uint32_t>(sy));
    }
    if (rotation != cache_[i].rotation) {
      cache_[i].rotation = rotation;
      lv_image_set_rotation(bubbles_[i], rotation);
    }
  }

  if (hint) {
    const Rect& r = game.bubble(static_cast<std::size_t>(target_index)).rect;
    const float pulse = ui::motion::glow(now_ms);
    const int size = 70 + static_cast<int>(std::lround(pulse * 5.0f));
    lv_obj_set_size(hint_ring_, size, size);
    lv_obj_set_pos(hint_ring_, r.x + r.w / 2 - size / 2, r.y + r.h / 2 - size / 2);
    lv_obj_set_style_border_opa(hint_ring_, static_cast<lv_opa_t>(80 + pulse * 120), 0);
    lv_obj_set_pos(helper_hand_, r.x + r.w - 8, r.y + r.h - 18);
  }
}

void ColorPopView::sync_effects(const ColorPopGame& game, uint32_t now_ms) {
  if (game.rounds_completed() != last_round_) {
    last_round_ = game.rounds_completed();
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      if (!game.bubble(i).popped) continue;
      const Rect& r = game.bubble(i).rect;
      const float cx = static_cast<float>(r.x + r.w / 2);
      const float cy = static_cast<float>(r.y + r.h / 2);
      fx_.burst(cx, cy, 18, ui::fx::sparkle());
      fx_.burst(cx, cy, 14, ui::fx::burst(hue_for(game.bubble(i).color), 5.0f));
      fx_.ring(cx, cy, 14.0f, 55.0f, 550, hue_for(game.bubble(i).color).light, 3);
      crab_spring_.kick(-1.4f);
      if (last_round_ % 5 == 0) fx_.burst(42.0f, 206.0f, 24, ui::fx::confetti());
      break;
    }
  }

  const bool hint = game.hint_active();
  if (hint && last_hint_ms_ == 0) {
    fx_.ring(160.0f, 32.0f, 25.0f, 38.0f, 420, ui::tok::kHintGlow, 3);
  }
  last_hint_ms_ = hint ? 1 : 0;

  const float dt = have_time_ ? static_cast<float>(now_ms - last_ms_) * 0.001f : 0.0f;
  have_time_ = true;
  last_ms_ = now_ms;
  crab_spring_.step(dt, 1.0f);
  const float bob = ui::motion::bob(now_ms, 0.1f, 1.5f);
  const int zoom = static_cast<int>(std::lround((1.0f + (crab_spring_.x - 1.0f) * 0.24f) * 256.0f));
  lv_obj_set_pos(crab_, 42 - kCrabW / 2, 205 - kCrabH / 2 + static_cast<int>(std::lround(bob)));
  lv_image_set_scale_x(crab_, static_cast<uint32_t>(zoom));
  lv_image_set_scale_y(crab_, static_cast<uint32_t>(512 - zoom));
  fx_.update(now_ms);
}

void ColorPopView::sync(const ColorPopGame& game, uint32_t now_ms) {
  if (game.phase() != last_phase_) {
    last_phase_ = game.phase();
    visible(intro_play_, game.phase() == Phase::INTRO);
  }
  sync_badge(game);
  sync_bubbles(game, now_ms);
  sync_effects(game, now_ms);
}

}  // namespace little_arcade::color_pop
