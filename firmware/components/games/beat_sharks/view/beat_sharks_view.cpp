#include "little_arcade/games/beat_sharks_view.hpp"

#include "little_arcade/games/beat_sharks_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::beat_sharks {

namespace {
using ui::to_lv;
namespace pal = ui::palette;
using G = BeatSharksGame;

constexpr int kFillMaxW = 36;
constexpr int kFinY = kScreenHeight - art::kFinH;
// Keep active-column feedback gentle and position-only (no pulsing/flashing).
constexpr int kHopPx = 2;
constexpr int kWavePx = 2;
constexpr uint32_t kWaveStepMs = 90;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t* image(lv_obj_t* parent, const ui::ArgbImage& img) {
  lv_obj_t* o = lv_image_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  if (img.ready()) lv_image_set_src(o, img.dsc());
  return o;
}

void set_src(lv_obj_t* o, const ui::ArgbImage& img) {
  if (o && img.ready()) lv_image_set_src(o, img.dsc());
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

void paint_icon(ui::ArgbImage& img, int w, int h, void (*painter)(ui::Canvas&)) {
  if (!img.allocate(w, h)) return;
  ui::Canvas c = img.canvas();
  painter(c);
}

// Side-to-side "nope, try again" shake that fades out; no failure sound.
int wiggle_dx(uint32_t ms) {
  if (ms == 0) return 0;
  static constexpr int kShake[] = {0, 6, 0, -6};
  return kShake[(ms / 50) % 4] * static_cast<int>(ms) / static_cast<int>(G::kWiggleMs);
}

int column_centre(int step) { return G::kCellX0 + G::kCellPitchX * step + G::kCellW / 2; }

// 0 red dot, 1 stop square (listening), 2 sparkle (built-in sounds).
int record_mode(const G& game) {
  if (game.phase() == Phase::RECORDING) return 1;
  return game.sparkle_mode() ? 2 : 0;
}
}  // namespace

void BeatSharksView::create(lv_obj_t* parent) {
  for (std::size_t r = 0; r < kRows; ++r) {
    for (uint8_t s = 0; s < kSoundsPerRow; ++s) {
      if (!sharks_[r][s].allocate(art::kSharkSize, art::kSharkSize)) continue;
      ui::Canvas c = sharks_[r][s].canvas();
      art::paint_shark(c, r, static_cast<uint8_t>(s + 1));
    }
  }
  paint_icon(bubble_, art::kSharkSize, art::kSharkSize, art::paint_bubble);
  paint_icon(play_icon_, art::kIconSize, art::kIconSize, art::paint_play);
  paint_icon(stop_icon_, art::kIconSize, art::kIconSize, art::paint_stop);
  paint_icon(record_icon_, art::kIconSize, art::kIconSize, art::paint_record_dot);
  paint_icon(record_stop_icon_, art::kIconSize, art::kIconSize, art::paint_record_stop);
  paint_icon(sparkle_icon_, art::kIconSize, art::kIconSize, art::paint_sparkle);
  paint_icon(tempo_icons_[0], art::kIconSize, art::kIconSize, art::paint_turtle);
  paint_icon(tempo_icons_[1], art::kIconSize, art::kIconSize, art::paint_walk_shark);
  paint_icon(tempo_icons_[2], art::kIconSize, art::kIconSize, art::paint_rabbit);
  paint_icon(fin_icon_, art::kFinW, art::kFinH, art::paint_fin);
  paint_icon(star_icon_, 26, 26, art::paint_star);

  lv_obj_set_style_bg_color(parent, to_lv(art::kWater), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::REEF);

  play_ = button(parent, G::kPlayButton, 4, pal::kWhite);
  play_img_ = image(play_, play_icon_);
  lv_obj_center(play_img_);

  record_ = button(parent, G::kRecordButton, 4, pal::kWhite);
  lv_obj_set_style_radius(record_, LV_RADIUS_CIRCLE, 0);
  record_img_ = image(record_, record_icon_);
  lv_obj_center(record_img_);
  record_fill_ = plain(record_);
  lv_obj_set_size(record_fill_, 2, 4);
  lv_obj_align(record_fill_, LV_ALIGN_BOTTOM_LEFT, 6, -4);
  lv_obj_set_style_radius(record_fill_, 2, 0);
  lv_obj_set_style_bg_color(record_fill_, to_lv(art::kRecordRed), 0);
  lv_obj_set_style_bg_opa(record_fill_, LV_OPA_COVER, 0);
  set_visible(record_fill_, false);

  tempo_ = button(parent, G::kTempoButton, 4, pal::kWhite);
  tempo_img_ = image(tempo_, tempo_icons_[G::kDefaultTempo]);
  lv_obj_center(tempo_img_);

  // Soft tint on the column the fin is on (position only, no flashing).
  column_ = plain(parent);
  lv_obj_set_size(column_, G::kCellPitchX, kScreenHeight - G::kCellY0 + 4);
  lv_obj_set_style_radius(column_, 8, 0);
  lv_obj_set_style_bg_color(column_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_bg_opa(column_, LV_OPA_50, 0);
  set_visible(column_, false);

  for (std::size_t r = 0; r < kRows; ++r) {
    lv_style_t& st = cell_styles_[r];
    lv_style_init(&st);
    lv_style_set_radius(&st, 10);
    lv_style_set_bg_color(&st, to_lv(art::kRowTints[r]));
    lv_style_set_bg_opa(&st, LV_OPA_80);
    lv_style_set_border_color(&st, to_lv(art::kRowColors[r]));
    lv_style_set_border_width(&st, 2);
    for (std::size_t s = 0; s < kSteps; ++s) {
      const Rect rect = G::cell_rect(r, s);
      lv_obj_t* o = plain(parent);
      lv_obj_add_style(o, &st, 0);
      lv_obj_set_pos(o, rect.x, rect.y);
      lv_obj_set_size(o, rect.w, rect.h);
      cells_[r][s] = o;
    }
  }

  fin_ = image(parent, fin_icon_);
  lv_obj_set_pos(fin_, column_centre(0) - art::kFinW / 2, kFinY);

  static constexpr int kStarX[] = {96, 140, 184};
  static constexpr int kStarY[] = {32, 30, 32};
  for (std::size_t i = 0; i < 3; ++i) {
    if (!star_icon_.ready()) break;
    stars_[i] = image(parent, star_icon_);
    lv_obj_set_pos(stars_[i], kStarX[i], kStarY[i]);
    set_visible(stars_[i], false);
  }
  initialised_ = false;
}

void BeatSharksView::sync(const BeatSharksGame& game) {
  if (!initialised_) {
    for (std::size_t r = 0; r < kRows; ++r)
      for (std::size_t s = 0; s < kSteps; ++s) {
        shown_cell_[r][s] = -1;
        shown_dy_[r][s] = 1 << 20;
      }
    shown_playing_ = shown_record_mode_ = shown_record_lit_ = shown_level_ = shown_fill_ = -1;
    shown_tempo_ = shown_fin_x_ = shown_stars_ = -1;
    shown_column_ = -2;
    shown_record_dx_ = 0;
    lv_obj_set_pos(record_, G::kRecordButton.x + 4, G::kRecordButton.y + 4);
    initialised_ = true;
  }
  const ISampler* sampler = game.sampler();
  const bool recording = game.phase() == Phase::RECORDING;
  const bool wave = game.celebration() == Celebration::FULL_OCEAN;
  const std::size_t wave_col = (game.phase_ms() / kWaveStepMs) % (kSteps + 2);

  for (std::size_t r = 0; r < kRows; ++r) {
    for (std::size_t s = 0; s < kSteps; ++s) {
      const int v = game.cell(r, s);
      if (v != shown_cell_[r][s]) {
        shown_cell_[r][s] = v;
        const ui::ArgbImage& img = v == 0 ? bubble_ : sharks_[r][v - 1];
        if (img.ready()) lv_obj_set_style_bg_image_src(cells_[r][s], img.dsc(), 0);
      }
      // A hop lifts the whole tile, so its shark visibly jumps.
      int dy = game.hopping(r, s) ? -kHopPx : 0;
      if (wave && v != 0 && wave_col == s) dy = -kWavePx;
      if (dy != shown_dy_[r][s]) {
        shown_dy_[r][s] = dy;
        lv_obj_set_y(cells_[r][s], G::cell_rect(r, s).y + dy);
      }
    }
  }

  const int column = game.step();
  if (column != shown_column_) {
    shown_column_ = column;
    set_visible(column_, column >= 0);
    if (column >= 0) lv_obj_set_pos(column_, G::kCellX0 + G::kCellPitchX * column - 2, G::kCellY0 - 4);
  }

  // The fin glides toward the next column; it rests on the last one until the loop wraps.
  const int step = column < 0 ? 0 : column;
  int fin_x = column_centre(step);
  if (game.playing() && step + 1 < static_cast<int>(kSteps)) {
    fin_x += G::kCellPitchX * game.step_progress_permille() / 1000;
  }
  if (fin_x != shown_fin_x_) {
    shown_fin_x_ = fin_x;
    lv_obj_set_pos(fin_, fin_x - art::kFinW / 2, kFinY);
  }

  const int playing = game.playing() ? 1 : 0;
  if (playing != shown_playing_) {
    shown_playing_ = playing;
    set_src(play_img_, playing ? stop_icon_ : play_icon_);
  }

  const int mode = record_mode(game);
  if (mode != shown_record_mode_) {
    shown_record_mode_ = mode;
    set_src(record_img_, mode == 1 ? record_stop_icon_ : (mode == 2 ? sparkle_icon_ : record_icon_));
    lv_obj_set_style_bg_color(record_, to_lv(recording ? 0xFFE3E3 : pal::kWhite), 0);
  }
  // Microphone level halo (aggregate loudness only) and a bar filling to the auto-stop.
  const int level = recording && sampler ? sampler->input_level() / 32 : 0;
  const int lit = game.record_lit() && !recording ? 1 : 0;
  if (level != shown_level_ || lit != shown_record_lit_) {
    shown_level_ = level;
    shown_record_lit_ = lit;
    lv_obj_set_style_outline_color(record_, to_lv(lit ? pal::kYellow : art::kRecordRed), 0);
    lv_obj_set_style_outline_opa(record_, static_cast<lv_opa_t>(lit ? LV_OPA_COVER : LV_OPA_60), 0);
    lv_obj_set_style_outline_width(record_, lit ? 4 : level, 0);
  }
  int fill = -1;
  if (recording && sampler) {
    const int p = sampler->record_progress_permille();
    fill = (p > G::kRecordStopPermille ? G::kRecordStopPermille : p) * kFillMaxW / G::kRecordStopPermille;
  }
  if (fill != shown_fill_) {
    shown_fill_ = fill;
    set_visible(record_fill_, fill >= 0);
    if (fill >= 0) lv_obj_set_width(record_fill_, fill < 2 ? 2 : fill);
  }
  const int dx = wiggle_dx(game.wiggle_ms());
  if (dx != shown_record_dx_) {
    shown_record_dx_ = dx;
    lv_obj_set_pos(record_, G::kRecordButton.x + 4 + dx, G::kRecordButton.y + 4);
  }

  const int tempo = game.tempo();
  if (tempo != shown_tempo_) {
    shown_tempo_ = tempo;
    set_src(tempo_img_, tempo_icons_[tempo]);
  }

  const int stars = game.celebration() == Celebration::NEW_SOUND ? 1 : 0;
  if (stars != shown_stars_) {
    shown_stars_ = stars;
    for (lv_obj_t* star : stars_) set_visible(star, stars != 0);
  }
}

}  // namespace little_arcade::beat_sharks
