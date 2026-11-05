#include "little_arcade/games/sound_jar_view.hpp"

#include "little_arcade/games/sound_jar_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"
// Not re-exported by lvgl.h in 9.5; needed to drop the stale waveform raster.
#include "src/misc/cache/instance/lv_image_cache.h"

namespace little_arcade::sound_jar {

namespace {
using ui::to_lv;
namespace pal = ui::palette;

constexpr int kInset = 3;
constexpr int kWaveX = 7;
constexpr int kWaveY = 7;
constexpr int kWaveW = SoundJarGame::kJar.w - 2 * kInset - 2 * kWaveX;
constexpr int kWaveH = SoundJarGame::kJar.h - 2 * kInset - 2 * kWaveY - 2;
constexpr int kFillY = kWaveY + kWaveH + 2;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
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

lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* src) {
  lv_obj_t* o = lv_image_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(o, src);
  return o;
}

lv_obj_t* button(lv_obj_t* parent, const Rect& r, int inset, uint32_t bg) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, r.x + inset, r.y + inset);
  lv_obj_set_size(o, r.w - 2 * inset, r.h - 2 * inset);
  lv_obj_set_style_radius(o, 10, 0);
  lv_obj_set_style_bg_color(o, to_lv(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, to_lv(pal::kChromeEdge), 0);
  lv_obj_set_style_border_width(o, 2, 0);
  lv_obj_set_style_outline_width(o, 0, 0);
  return o;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (!o) return;
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

void paint_icon(ui::ArgbImage& img, int size, void (*painter)(ui::Canvas&)) {
  if (!img.allocate(size, size)) return;
  ui::Canvas c = img.canvas();
  painter(c);
}

// Side-to-side "nope, try again" shake that fades out; no failure sound.
int wiggle_dx(uint32_t ms) {
  if (ms == 0) return 0;
  static constexpr int kSteps[] = {0, 6, 0, -6};
  return kSteps[(ms / 50) % 4] * static_cast<int>(ms) / static_cast<int>(SoundJarGame::kWiggleMs);
}
}  // namespace

void SoundJarView::create(lv_obj_t* parent) {
  for (std::size_t i = 0; i < kPadCount; ++i) {
    if (critter_icons_[i].allocate(art::kCritterSize, art::kCritterSize)) {
      ui::Canvas c = critter_icons_[i].canvas();
      art::paint_critter(c, i);
    }
  }
  for (std::size_t i = 0; i < kEffectCount; ++i) {
    if (effect_icons_[i].allocate(art::kIconSize, art::kIconSize)) {
      ui::Canvas c = effect_icons_[i].canvas();
      art::paint_effect_icon(c, i);
    }
  }
  paint_icon(record_icon_, art::kIconSize, art::paint_record_dot);
  paint_icon(stop_icon_, art::kIconSize, art::paint_stop_square);
  paint_icon(sparkle_icon_, art::kIconSize, art::paint_sparkle);
  paint_icon(star_icon_, 28, art::paint_star);
  wave_.allocate(kWaveW, kWaveH);

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  band(parent, 0, 60, pal::kSkyDark);
  band(parent, kScreenHeight - 6, 6, pal::kGround);

  record_ = button(parent, SoundJarGame::kRecordButton, 4, pal::kWhite);
  lv_obj_set_style_radius(record_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_outline_color(record_, to_lv(art::kRecordRed), 0);
  lv_obj_set_style_outline_opa(record_, LV_OPA_60, 0);
  if (record_icon_.ready()) {
    record_dot_ = image(record_, record_icon_.dsc());
    lv_obj_center(record_dot_);
  }
  if (stop_icon_.ready()) {
    stop_square_ = image(record_, stop_icon_.dsc());
    lv_obj_center(stop_square_);
  }
  if (sparkle_icon_.ready()) {
    sparkle_ = image(record_, sparkle_icon_.dsc());
    lv_obj_center(sparkle_);
  }

  jar_ = button(parent, SoundJarGame::kJar, kInset, art::kJarGlass);
  lv_obj_set_style_radius(jar_, 14, 0);
  lv_obj_set_style_border_color(jar_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(jar_, 3, 0);
  lv_obj_set_style_outline_color(jar_, to_lv(pal::kYellow), 0);
  if (wave_.ready()) {
    wave_obj_ = image(jar_, wave_.dsc());
    lv_obj_set_pos(wave_obj_, kWaveX - 3, kWaveY - 3);
  }
  playhead_ = plain(jar_);
  lv_obj_set_size(playhead_, 3, kWaveH + 4);
  lv_obj_set_style_radius(playhead_, 2, 0);
  lv_obj_set_style_bg_color(playhead_, to_lv(pal::kInk), 0);
  lv_obj_set_style_bg_opa(playhead_, LV_OPA_COVER, 0);
  set_visible(playhead_, false);
  fill_ = plain(jar_);
  lv_obj_set_pos(fill_, kWaveX - 3, kFillY - 3);
  lv_obj_set_size(fill_, 1, 4);
  lv_obj_set_style_radius(fill_, 2, 0);
  lv_obj_set_style_bg_color(fill_, to_lv(art::kRecordRed), 0);
  lv_obj_set_style_bg_opa(fill_, LV_OPA_COVER, 0);
  set_visible(fill_, false);

  for (std::size_t i = 0; i < kEffectCount; ++i) {
    lv_obj_t* o = button(parent, SoundJarGame::kEffects[i], 2, pal::kWhite);
    effects_[i] = o;
    if (effect_icons_[i].ready()) lv_obj_center(image(o, effect_icons_[i].dsc()));
  }

  for (std::size_t i = 0; i < kPadCount; ++i) {
    lv_obj_t* o = button(parent, SoundJarGame::kPads[i], 2, art::kPadTints[i]);
    lv_obj_set_style_radius(o, 14, 0);
    lv_obj_set_style_border_color(o, to_lv(art::kCritterColors[i]), 0);
    lv_obj_set_style_border_width(o, 3, 0);
    lv_obj_set_style_outline_color(o, to_lv(pal::kWhite), 0);
    pads_[i] = o;
    if (critter_icons_[i].ready()) {
      lv_obj_t* icon = image(o, critter_icons_[i].dsc());
      lv_obj_align(icon, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
  }
  bend_marker_ = plain(parent);
  lv_obj_set_size(bend_marker_, 16, 16);
  lv_obj_set_style_radius(bend_marker_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bend_marker_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_bg_opa(bend_marker_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(bend_marker_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(bend_marker_, 3, 0);
  set_visible(bend_marker_, false);

  static constexpr int kStarX[] = {140, 180, 220};
  static constexpr int kStarY[] = {14, 4, 14};
  for (std::size_t i = 0; i < 3; ++i) {
    if (!star_icon_.ready()) break;
    stars_[i] = image(parent, star_icon_.dsc());
    lv_obj_set_pos(stars_[i], kStarX[i], kStarY[i]);
    set_visible(stars_[i], false);
  }
  initialised_ = false;
}

void SoundJarView::repaint_wave(const SoundJarGame& game, bool reversed) {
  if (!wave_.ready()) return;
  uint8_t columns[ISampler::kThumbColumns]{};
  if (const ISampler* s = game.sampler()) {
    for (std::size_t i = 0; i < ISampler::kThumbColumns; ++i) columns[i] = s->thumb(i);
  }
  ui::Canvas c = wave_.canvas();
  art::paint_wave(c, columns, ISampler::kThumbColumns, reversed, art::kWaveColor);
  lv_image_cache_drop(wave_.dsc());
  if (wave_obj_) lv_obj_invalidate(wave_obj_);
}

void SoundJarView::sync(const SoundJarGame& game) {
  if (!initialised_) {
    shown_generation_ = ~0u;
    shown_reversed_ = -1;
    shown_phase_ = -1;
    shown_sparkle_ = -1;
    shown_effects_ = -1;
    shown_level_ = -1;
    shown_fill_ = -1;
    shown_playhead_ = -2;
    shown_bend_ = -2;
    shown_jar_dx_ = 0;
    shown_jar_lit_ = -1;
    for (std::size_t i = 0; i < kPadCount; ++i) shown_pad_lit_[i] = !game.pad_lit(i);
    lv_obj_set_pos(jar_, SoundJarGame::kJar.x + kInset, SoundJarGame::kJar.y + kInset);
    initialised_ = true;
  }
  const ISampler* sampler = game.sampler();
  const bool recording = game.phase() == Phase::RECORDING;

  const int phase = static_cast<int>(game.phase());
  const int sparkle = game.sparkle_mode() ? 1 : 0;
  if (phase != shown_phase_ || sparkle != shown_sparkle_) {
    shown_phase_ = phase;
    shown_sparkle_ = sparkle;
    set_visible(sparkle_, sparkle && !recording);
    set_visible(record_dot_, !sparkle && !recording);
    set_visible(stop_square_, recording);
    lv_obj_set_style_bg_color(record_, to_lv(recording ? 0xFFE3E3 : pal::kWhite), 0);
    for (lv_obj_t* star : stars_) set_visible(star, game.phase() == Phase::CELEBRATE);
  }

  // Microphone level halo around the Record button (aggregate loudness only).
  const int level = recording && sampler ? sampler->input_level() / 32 : 0;
  if (level != shown_level_) {
    shown_level_ = level;
    lv_obj_set_style_outline_width(record_, level, 0);
  }

  // The jar "fills up" while recording.
  const int fill = recording && sampler ? sampler->record_progress_permille() * kWaveW / 1000 : -1;
  if (fill != shown_fill_) {
    shown_fill_ = fill;
    set_visible(fill_, fill >= 0);
    if (fill >= 0) lv_obj_set_width(fill_, fill < 2 ? 2 : fill);
  }

  const int reversed = game.effect_on(static_cast<std::size_t>(Effect::BACKWARDS)) ? 1 : 0;
  const uint32_t generation = sampler ? sampler->clip_generation() : 0;
  if (generation != shown_generation_ || reversed != shown_reversed_) {
    shown_generation_ = generation;
    shown_reversed_ = reversed;
    repaint_wave(game, reversed != 0);
  }

  const int head = sampler && !recording ? sampler->playhead_permille() : -1;
  const int head_x = head < 0 ? -1 : kWaveX - 3 + head * (kWaveW - 3) / 1000;
  if (head_x != shown_playhead_) {
    shown_playhead_ = head_x;
    set_visible(playhead_, head_x >= 0);
    if (head_x >= 0) lv_obj_set_pos(playhead_, head_x, kWaveY - 5);
  }

  const int dx = wiggle_dx(game.wiggle_ms());
  if (dx != shown_jar_dx_) {
    shown_jar_dx_ = dx;
    lv_obj_set_pos(jar_, SoundJarGame::kJar.x + kInset + dx, SoundJarGame::kJar.y + kInset);
  }
  const int jar_lit = game.jar_lit() ? 1 : 0;
  if (jar_lit != shown_jar_lit_) {
    shown_jar_lit_ = jar_lit;
    lv_obj_set_style_outline_width(jar_, jar_lit ? 4 : 0, 0);
  }

  const int effects = game.effects();
  if (effects != shown_effects_) {
    shown_effects_ = effects;
    for (std::size_t i = 0; i < kEffectCount; ++i) {
      const bool on = game.effect_on(i);
      lv_obj_set_style_bg_color(effects_[i], to_lv(on ? 0xFFF4C2 : pal::kWhite), 0);
      lv_obj_set_style_border_color(effects_[i], to_lv(on ? pal::kOrange : pal::kChromeEdge), 0);
      lv_obj_set_style_border_width(effects_[i], on ? 4 : 2, 0);
    }
  }

  for (std::size_t i = 0; i < kPadCount; ++i) {
    const bool lit = game.pad_lit(i);
    if (lit == shown_pad_lit_[i]) continue;
    shown_pad_lit_[i] = lit;
    lv_obj_set_style_outline_width(pads_[i], lit ? 4 : 0, 0);
    lv_obj_set_style_bg_color(pads_[i], to_lv(lit ? pal::kWhite : art::kPadTints[i]), 0);
  }

  // A dot rides along the pads while a finger slides (pitch bend).
  const int bend = game.bend_position();
  if (bend != shown_bend_) {
    shown_bend_ = bend;
    set_visible(bend_marker_, bend >= 0);
    if (bend >= 0) {
      const Rect& first = SoundJarGame::kPads[0];
      const int cx = first.x + first.w / 2 + bend * (SoundJarGame::kPads[1].x - first.x) / 100;
      lv_obj_set_pos(bend_marker_, cx - 8, first.y + 6);
    }
  }
}

}  // namespace little_arcade::sound_jar
