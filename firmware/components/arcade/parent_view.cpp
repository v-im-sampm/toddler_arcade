#include "little_arcade/parent_view.hpp"

#include <cstdio>

#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade {

namespace {
namespace pal = ui::palette;
using settings::ParentGate;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t* text(lv_obj_t* parent, const char* s, bool big) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_color(l, ui::to_lv(pal::kInk), 0);
#if LV_FONT_MONTSERRAT_28
  if (big) lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
#else
  (void)big;
#endif
  lv_label_set_text(l, s);
  return l;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}
}  // namespace

constexpr int16_t kHoldBarW = 48, kHoldBarH = 8;
constexpr int16_t kHoldBarX = ParentGate::kHotspot.x + (ParentGate::kHotspot.w - kHoldBarW) / 2;
constexpr int16_t kHoldBarY = ParentGate::kHotspot.y + ParentGate::kHotspot.h + 4;

lv_obj_t* ParentView::button(lv_obj_t* parent, const Rect& r, lv_obj_t** label_out) {
  lv_obj_t* b = plain(parent);
  lv_obj_set_pos(b, r.x, r.y);
  lv_obj_set_size(b, r.w, r.h);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, ui::to_lv(pal::kWhite), 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::to_lv(pal::kChromeEdge), 0);
  lv_obj_t* l = text(b, "", true);
  lv_obj_center(l);
  if (label_out) *label_out = l;
  return b;
}

void ParentView::create(lv_obj_t* screen) {
  root_ = plain(screen);
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_size(root_, kScreenWidth, kScreenHeight);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(root_, ui::to_lv(pal::kSkyDark), 0);

  challenge_ = plain(root_);
  lv_obj_set_size(challenge_, kScreenWidth, kScreenHeight);
  lv_obj_t* title = text(challenge_, "Grown-ups: sound settings", false);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
  question_ = text(challenge_, "", true);
  lv_obj_align(question_, LV_ALIGN_TOP_MID, 0, 56);
  for (int i = 0; i < ParentGate::kChoiceCount; ++i) button(challenge_, ParentGate::kChoiceRects[i], &choice_labels_[i]);

  settings_ = plain(root_);
  lv_obj_set_size(settings_, kScreenWidth, kScreenHeight);
  lv_obj_t* vol = text(settings_, LV_SYMBOL_VOLUME_MAX "  Volume (0 = off)", false);
  lv_obj_align(vol, LV_ALIGN_TOP_MID, 0, 40);
  for (int l = 0; l < ParentGate::kLevelCount; ++l) {
    lv_obj_t* label = nullptr;
    level_buttons_[l] = button(settings_, ParentGate::level_rect(l), &label);
    char s[4];
    std::snprintf(s, sizeof(s), "%d", l);
    lv_label_set_text(label, s);
  }
  lv_obj_t* done_label = nullptr;
  button(settings_, ParentGate::kDoneRect, &done_label);
  lv_label_set_text(done_label, LV_SYMBOL_OK);
  mic_button_ = button(settings_, ParentGate::kMicToggleRect, &mic_label_);
  lv_obj_set_style_text_font(mic_label_, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_align(mic_label_, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  shown_ = ParentGate::State::CLOSED;

  hold_track_ = plain(screen);
  lv_obj_set_pos(hold_track_, kHoldBarX, kHoldBarY);
  lv_obj_set_size(hold_track_, kHoldBarW, kHoldBarH);
  lv_obj_set_style_radius(hold_track_, kHoldBarH / 2, 0);
  lv_obj_set_style_bg_opa(hold_track_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(hold_track_, ui::to_lv(pal::kWhite), 0);
  lv_obj_set_style_border_width(hold_track_, 1, 0);
  lv_obj_set_style_border_color(hold_track_, ui::to_lv(pal::kChromeEdge), 0);
  hold_fill_ = plain(hold_track_);
  lv_obj_set_pos(hold_fill_, 0, 0);
  lv_obj_set_size(hold_fill_, 0, kHoldBarH);
  lv_obj_set_style_radius(hold_fill_, kHoldBarH / 2, 0);
  lv_obj_set_style_bg_opa(hold_fill_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(hold_fill_, ui::to_lv(pal::kGreen), 0);
  lv_obj_add_flag(hold_track_, LV_OBJ_FLAG_HIDDEN);
}

void ParentView::sync(const ParentGate& gate) {
  const uint32_t held = gate.hold_ms();
  const int hold_px = held < ParentGate::kShowProgressMs
                          ? -1
                          : static_cast<int>((held - ParentGate::kShowProgressMs) * kHoldBarW /
                                             (ParentGate::kHoldMs - ParentGate::kShowProgressMs));
  if (hold_px != shown_hold_px_) {
    if (shown_hold_px_ < 0) lv_obj_move_foreground(hold_track_);
    shown_hold_px_ = hold_px;
    set_visible(hold_track_, hold_px >= 0);
    if (hold_px >= 0) lv_obj_set_width(hold_fill_, static_cast<int32_t>(hold_px > kHoldBarW ? kHoldBarW : hold_px));
  }

  const ParentGate::State s = gate.state();
  if (s != shown_) {
    shown_ = s;
    set_visible(root_, s != ParentGate::State::CLOSED);
    set_visible(challenge_, s == ParentGate::State::CHALLENGE);
    set_visible(settings_, s == ParentGate::State::SETTINGS);
    if (s == ParentGate::State::CLOSED) return;
    shown_a_ = shown_b_ = 0;  // force a full refresh on every opening
    shown_level_ = -1;
    shown_mic_ = -1;
    lv_obj_move_foreground(root_);
  }
  if (s == ParentGate::State::CHALLENGE && (gate.sum_a() != shown_a_ || gate.sum_b() != shown_b_)) {
    shown_a_ = gate.sum_a();
    shown_b_ = gate.sum_b();
    lv_label_set_text_fmt(question_, "%u + %u = ?", static_cast<unsigned>(shown_a_), static_cast<unsigned>(shown_b_));
    for (int i = 0; i < ParentGate::kChoiceCount; ++i) {
      lv_label_set_text_fmt(choice_labels_[i], "%u", static_cast<unsigned>(gate.choice(i)));
    }
  }
  if (s == ParentGate::State::SETTINGS && gate.level() != shown_level_) {
    shown_level_ = gate.level();
    for (int l = 0; l < ParentGate::kLevelCount; ++l) {
      const bool on = l == shown_level_;
      lv_obj_set_style_bg_color(level_buttons_[l], ui::to_lv(on ? pal::kGreen : pal::kWhite), 0);
      lv_obj_set_style_border_width(level_buttons_[l], on ? 4 : 2, 0);
    }
  }
  const int mic = gate.mic_enabled() ? 1 : 0;
  if (s == ParentGate::State::SETTINGS && mic != shown_mic_) {
    shown_mic_ = mic;
    // Sound Jar and Beat Sharks may record a short clip into RAM only; never saved or sent.
    lv_label_set_text(mic_label_, mic ? "Mic\nON" : "Mic\nOFF");
    lv_obj_center(mic_label_);
    lv_obj_set_style_bg_color(mic_button_, ui::to_lv(mic ? pal::kGreen : pal::kWhite), 0);
    lv_obj_set_style_border_width(mic_button_, mic ? 4 : 2, 0);
  }
}

}  // namespace little_arcade
