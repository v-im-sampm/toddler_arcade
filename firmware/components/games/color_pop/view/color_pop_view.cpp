#include "little_arcade/games/color_pop_view.hpp"

#include <cmath>

#include "little_arcade/games/color_pop_patterns.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::color_pop {

namespace {
using ui::to_lv;
namespace pal = ui::palette;

constexpr float kPi = 3.14159265f;
constexpr int kBubbleInset = 2;  // body diameter = slot size - 2 * inset
constexpr int kShadowDx = 3;
constexpr int kShadowDy = 4;

uint32_t color_rgb(Color c) {
  switch (c) {
    case Color::RED:
      return pal::kRed;
    case Color::BLUE:
      return pal::kBlue;
    case Color::GREEN:
      return pal::kGreen;
    case Color::YELLOW:
      return pal::kYellow;
  }
  return pal::kWhite;
}

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

lv_obj_t* band(lv_obj_t* parent, int y, int h, uint32_t rgb) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, 0, y);
  lv_obj_set_size(o, kScreenWidth, h);
  lv_obj_set_style_bg_color(o, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}

lv_obj_t* a8_image(lv_obj_t* parent, const lv_image_dsc_t* src, uint32_t rgb) {
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(img, src);
  lv_obj_set_style_image_recolor(img, to_lv(rgb), 0);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
  return img;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

// Soft outline pulse used for hints (no rapid flashing: ~1.3 Hz).
int16_t pulse_pad(uint32_t now_ms) {
  return static_cast<int16_t>(3 + std::lround(2.0f * std::sin(static_cast<float>(now_ms) / 120.0f)));
}
}  // namespace

void ColorPopView::create(lv_obj_t* parent) {
  const float b = static_cast<float>(kPatternPx);
  for (std::size_t c = 0; c < kColorCount; ++c) {
    const Pattern p = pattern_for(static_cast<Color>(c));
    patterns_[c].build([p, b](float x, float y) { return pattern_inside(p, x, y, b); });
  }

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  band(parent, 0, 62, pal::kSkyDark);
  band(parent, kScreenHeight - 10, 10, pal::kGround);

  // Target badge: requested colour + pattern, so play works with audio off.
  const Rect& br = ColorPopGame::kTargetBadge;
  badge_ = disc(parent, br.w, pal::kWhite);
  lv_obj_set_pos(badge_, br.x, br.y);
  lv_obj_set_style_border_color(badge_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(badge_, 2, 0);
  lv_obj_set_style_outline_color(badge_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_outline_width(badge_, 0, 0);
  badge_fill_ = disc(badge_, br.w - 12, pal::kWhite);
  lv_obj_center(badge_fill_);
  badge_icon_ = a8_image(badge_fill_, &patterns_[0].dsc, pal::kWhite);
  lv_image_set_scale(badge_icon_, 160);  // ~25 px pattern inside a 44 px disc
  lv_obj_center(badge_icon_);
  badge_question_ = lv_label_create(badge_);
  lv_label_set_text_static(badge_question_, "?");
#if LV_FONT_MONTSERRAT_28
  lv_obj_set_style_text_font(badge_question_, &lv_font_montserrat_28, 0);
#endif
  lv_obj_set_style_text_color(badge_question_, to_lv(pal::kInk), 0);
  lv_obj_center(badge_question_);
  lv_obj_t* speaker = lv_label_create(parent);
  lv_label_set_text_static(speaker, LV_SYMBOL_VOLUME_MAX);
  lv_obj_set_style_text_color(speaker, to_lv(pal::kInk), 0);
  lv_obj_set_pos(speaker, br.x + br.w + 4, br.y + br.h / 2 - 8);

  const int d = ColorPopGame::kSlots[0].w - 2 * kBubbleInset;
  for (auto& bo : bubbles_) {
    bo.shadow = disc(parent, d, pal::kInk, LV_OPA_20);
    bo.body = disc(parent, d, pal::kWhite);
    lv_obj_set_style_border_color(bo.body, to_lv(pal::kInk), 0);
    lv_obj_set_style_border_width(bo.body, 2, 0);
    lv_obj_set_style_outline_color(bo.body, to_lv(pal::kWhite), 0);
    lv_obj_set_style_outline_width(bo.body, 0, 0);
    lv_obj_t* shine = disc(bo.body, 8, pal::kWhite);
    lv_obj_align(shine, LV_ALIGN_CENTER, -d * 3 / 10, -d * 9 / 40);
    bo.icon = a8_image(bo.body, &patterns_[0].dsc, pal::kWhite);
    lv_obj_align(bo.icon, LV_ALIGN_CENTER, 0, 2);
  }

  ring_ = disc(parent, 20, pal::kWhite, LV_OPA_TRANSP);
  lv_obj_set_style_border_color(ring_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_border_width(ring_, 3, 0);
  static constexpr uint32_t kSparkleColors[] = {pal::kRed, pal::kBlue, pal::kGreen, pal::kYellow, pal::kWhite};
  for (int i = 0; i < kSparkles; ++i) {
    sparkles_[i] = plain(parent);
    lv_obj_set_style_bg_color(sparkles_[i], to_lv(kSparkleColors[i % 5]), 0);
    lv_obj_set_style_bg_opa(sparkles_[i], LV_OPA_COVER, 0);
  }
  for (auto& c : cache_) c = BubbleCache{};
  initialised_ = false;
}

void ColorPopView::sync(const ColorPopGame& game, uint32_t now_ms) {
  if (!initialised_ || game.phase() != last_phase_) {
    last_phase_ = game.phase();
    phase_start_ms_ = now_ms;
  }
  const int wig = game.wiggling();
  if (wig != last_wiggle_) {
    last_wiggle_ = wig;
    wiggle_start_ms_ = now_ms;
  }
  initialised_ = true;

  sync_badge(game, now_ms);
  for (std::size_t i = 0; i < ColorPopGame::kMaxBubbles; ++i) sync_bubble(i, game, now_ms);
  sync_sparkles(game, now_ms);
}

void ColorPopView::sync_badge(const ColorPopGame& game, uint32_t now_ms) {
  const bool intro = game.phase() == Phase::INTRO;
  const int color = intro ? -1 : static_cast<int>(game.target());
  if (color != badge_color_) {
    badge_color_ = color;
    set_visible(badge_question_, intro);
    set_visible(badge_fill_, !intro);
    if (!intro) {
      lv_obj_set_style_bg_color(badge_fill_, to_lv(color_rgb(game.target())), 0);
      lv_image_set_src(badge_icon_, &patterns_[color].dsc);
    }
  }
  const int16_t pad = game.hint_active() ? pulse_pad(now_ms) : 0;
  if (pad != badge_pad_) {
    badge_pad_ = pad;
    lv_obj_set_style_outline_width(badge_, pad ? 3 : 0, 0);
    lv_obj_set_style_outline_pad(badge_, pad, 0);
  }
}

void ColorPopView::sync_bubble(std::size_t i, const ColorPopGame& game, uint32_t now_ms) {
  BubbleObjs& o = bubbles_[i];
  BubbleCache& c = cache_[i];
  const bool exists = i < game.bubble_count();
  const bool visible = exists && !game.bubble(i).popped;
  if (visible != c.visible) {
    c.visible = visible;
    set_visible(o.shadow, visible);
    set_visible(o.body, visible);
  }
  if (!visible) return;

  const Bubble& b = game.bubble(i);
  const int color = static_cast<int>(b.color);
  if (color != c.color) {
    c.color = color;
    lv_obj_set_style_bg_color(o.body, to_lv(color_rgb(b.color)), 0);
    lv_image_set_src(o.icon, &patterns_[color].dsc);
  }
  if (b.rect.x != c.x || b.rect.y != c.y) {
    c.x = b.rect.x;
    c.y = b.rect.y;
    lv_obj_set_pos(o.body, c.x + kBubbleInset, c.y + kBubbleInset);
    lv_obj_set_pos(o.shadow, c.x + kBubbleInset + kShadowDx, c.y + kBubbleInset + kShadowDy);
  }

  // Gentle "not me" wiggle: decaying sideways nudge, max 3 px.
  int16_t dx = 0;
  if (static_cast<int>(i) == last_wiggle_) {
    const float t = 1.0f - static_cast<float>(now_ms - wiggle_start_ms_) / ColorPopGame::kWiggleMs;
    if (t > 0.0f) dx = static_cast<int16_t>(std::lround(std::sin(t * kPi * 6.0f) * 3.0f * t));
  }
  if (dx != c.dx) {
    c.dx = dx;
    lv_obj_set_style_translate_x(o.body, dx, 0);
    lv_obj_set_style_translate_x(o.shadow, dx, 0);
  }

  const bool glow = game.hint_active() && game.phase() == Phase::PLAY && b.color == game.target();
  const int16_t pad = glow ? pulse_pad(now_ms) : 0;
  if (pad != c.glow_pad) {
    c.glow_pad = pad;
    lv_obj_set_style_outline_width(o.body, glow ? 4 : 0, 0);
    lv_obj_set_style_outline_pad(o.body, pad, 0);
  }
}

void ColorPopView::sync_sparkles(const ColorPopGame& game, uint32_t now_ms) {
  int popped = -1;
  if (game.phase() == Phase::CELEBRATE) {
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      if (game.bubble(i).popped) popped = static_cast<int>(i);
    }
  }
  const bool show = popped >= 0;
  if (show != sparkles_visible_) {
    sparkles_visible_ = show;
    set_visible(ring_, show);
    for (lv_obj_t* s : sparkles_) set_visible(s, show);
  }
  if (!show) return;

  const Rect& r = game.bubble(static_cast<std::size_t>(popped)).rect;
  const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  float t = static_cast<float>(now_ms - phase_start_ms_) / (ColorPopGame::kCelebrateMs * 0.7f);
  t = t > 1.0f ? 1.0f : t;
  const int size = static_cast<int>(5.0f * (1.0f - t)) + 1;
  const float dist = 10.0f + t * 50.0f;
  for (int i = 0; i < kSparkles; ++i) {
    const float a = static_cast<float>(i) * 2.0f * kPi / kSparkles;
    lv_obj_set_size(sparkles_[i], size, size);
    lv_obj_set_pos(sparkles_[i], cx + static_cast<int>(std::lround(std::cos(a) * dist)) - size / 2,
                   cy + static_cast<int>(std::lround(std::sin(a) * dist)) - size / 2);
  }
  const int rd = 20 + static_cast<int>(t * 60.0f);
  lv_obj_set_size(ring_, rd, rd);
  lv_obj_set_pos(ring_, cx - rd / 2, cy - rd / 2);
  lv_obj_set_style_border_opa(ring_, static_cast<lv_opa_t>(255.0f * (1.0f - t)), 0);
}

}  // namespace little_arcade::color_pop
