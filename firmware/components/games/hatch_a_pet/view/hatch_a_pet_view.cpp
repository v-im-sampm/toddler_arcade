#include "little_arcade/games/hatch_a_pet_view.hpp"

#include <cmath>

#include "little_arcade/games/hatch_a_pet_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::hatch_a_pet {

namespace {
using ui::to_lv;
namespace pal = ui::palette;
using G = HatchAPetGame;

constexpr float kPi = 3.14159265f;
constexpr float kRadToDeciDeg = 1800.0f / kPi;

// Screen geometry derived from the game's hit rects.
constexpr int32_t kEggCx = G::kEgg.x + G::kEgg.w / 2;
constexpr int32_t kEggImgX = kEggCx - art::kEggImgW / 2;
constexpr int32_t kEggImgY = G::kEgg.y - 2;  // the art draws the egg 2 px down
constexpr int32_t kEggBottom = G::kEgg.y + G::kEgg.h;
constexpr int32_t kCreatureCx = G::kCreature.x + G::kCreature.w / 2;
constexpr int32_t kCreatureCy = G::kCreature.y + G::kCreature.h / 2;
constexpr int kGlowPad = 8;  // glow image margin around the egg image
constexpr int kHeartW = 34;
constexpr int kHeartH = 36;
constexpr float kHeartSize = 30.0f;
constexpr int32_t kBadgeIconScale = 210;  // ~0.82: 34 x 42 icon -> 28 x 34 in the badge

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

lv_obj_t* image(lv_obj_t* parent) {
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  return img;
}

lv_obj_t* card(lv_obj_t* parent, const Rect& r) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, r.x + 4, r.y + 4);
  lv_obj_set_size(o, r.w - 8, r.h - 8);
  lv_obj_set_style_radius(o, 10, 0);
  lv_obj_set_style_bg_color(o, to_lv(pal::kChrome), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, to_lv(pal::kChromeEdge), 0);
  lv_obj_set_style_border_width(o, 2, 0);
  return o;
}

lv_obj_t* crack_line(lv_obj_t* parent, int width) {
  lv_obj_t* l = lv_line_create(parent);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_line_color(l, to_lv(pal::kInk), 0);
  lv_obj_set_style_line_width(l, width, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  return l;
}

int32_t round_i(float v) { return static_cast<int32_t>(std::lround(v)); }
float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
// Soft outline pulse used for hints (no rapid flashing: ~1.3 Hz).
float pulse(uint32_t now_ms) { return std::sin(static_cast<float>(now_ms) / 120.0f); }
}  // namespace

// ---------------------------------------------------------------- Sprite

void HatchAPetView::Sprite::show(bool v) {
  if (v == visible) return;
  visible = v;
  if (v) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}
void HatchAPetView::Sprite::pos(int32_t nx, int32_t ny) {
  if (nx == x && ny == y) return;
  x = nx;
  y = ny;
  lv_obj_set_pos(obj, x, y);
}
void HatchAPetView::Sprite::size(int32_t nw, int32_t nh) {
  if (nw == w && nh == h) return;
  w = nw;
  h = nh;
  lv_obj_set_size(obj, w, h);
}
void HatchAPetView::Sprite::rotation(int32_t deci_deg) {
  if (deci_deg == rot) return;
  rot = deci_deg;
  lv_image_set_rotation(obj, rot);
}
void HatchAPetView::Sprite::scale(int32_t nsx, int32_t nsy) {
  if (nsx == sx && nsy == sy) return;
  sx = nsx;
  sy = nsy;
  lv_image_set_scale_x(obj, static_cast<uint32_t>(sx));
  lv_image_set_scale_y(obj, static_cast<uint32_t>(sy));
}
void HatchAPetView::Sprite::image_opa(int32_t o) {
  if (o == opa) return;
  opa = o;
  lv_obj_set_style_image_opa(obj, static_cast<lv_opa_t>(o), 0);
}
void HatchAPetView::Sprite::bg_opa(int32_t o) {
  if (o == opa) return;
  opa = o;
  lv_obj_set_style_bg_opa(obj, static_cast<lv_opa_t>(o), 0);
}
void HatchAPetView::Sprite::source(const lv_image_dsc_t* dsc) {
  if (dsc == src) return;
  src = dsc;
  lv_image_set_src(obj, dsc);
}

// ---------------------------------------------------------------- art cache

const lv_image_dsc_t* HatchAPetView::egg(EggPattern p, int piece, bool allow_paint) {
  const auto pi = static_cast<std::size_t>(p);
  ui::ArgbImage& img = eggs_[pi][piece];
  if (!egg_painted_[pi][piece]) {
    if (!allow_paint) return nullptr;
    egg_painted_[pi][piece] = true;  // attempt once, even if memory is short
    if (!img.allocate(art::kEggImgW, art::kEggImgH)) return nullptr;
    const uint32_t t0 = lv_tick_get();
    ui::Canvas c = img.canvas();
    art::paint_egg_image(c, p, static_cast<art::Piece>(piece));
    const uint32_t ms = lv_tick_elaps(t0);
    ++rasters_;
    total_raster_ms_ += ms;
    max_raster_ms_ = ms > max_raster_ms_ ? ms : max_raster_ms_;
  }
  return img.ready() ? img.dsc() : nullptr;
}

const lv_image_dsc_t* HatchAPetView::creature(Creature k, bool allow_paint) {
  const auto ki = static_cast<std::size_t>(k);
  ui::ArgbImage& img = creatures_[ki];
  if (!creature_painted_[ki]) {
    if (!allow_paint) return nullptr;
    creature_painted_[ki] = true;
    if (!img.allocate(art::kCreatureImg, art::kCreatureImg)) return nullptr;
    const uint32_t t0 = lv_tick_get();
    ui::Canvas c = img.canvas();
    art::paint_creature_image(c, k);
    const uint32_t ms = lv_tick_elaps(t0);
    ++rasters_;
    total_raster_ms_ += ms;
    max_raster_ms_ = ms > max_raster_ms_ ? ms : max_raster_ms_;
  }
  return img.ready() ? img.dsc() : nullptr;
}

// While the child is tapping, paint (at most one per frame) what the hatch
// will need, so the reveal never stalls.
bool HatchAPetView::prewarm(const HatchAPetGame& game) {
  const auto pi = static_cast<std::size_t>(game.egg_pattern());
  for (const art::Piece piece : {art::Piece::WHOLE, art::Piece::TOP, art::Piece::BOTTOM}) {
    const int i = static_cast<int>(piece);
    if (!egg_painted_[pi][i]) return egg(game.egg_pattern(), i, true) != nullptr;
  }
  if (!creature_painted_[static_cast<std::size_t>(game.creature())]) return creature(game.creature(), true) != nullptr;
  return false;
}

// ---------------------------------------------------------------- create

void HatchAPetView::create(lv_obj_t* parent) {
  for (std::size_t p = 0; p < kEggPatternCount; ++p) {
    if (icons_[p].allocate(art::kEggIconW, art::kEggIconH)) {
      ui::Canvas c = icons_[p].canvas();
      art::paint_egg_icon(c, static_cast<EggPattern>(p));
    }
  }
  // Hint glow: a white ring just outside the egg.
  if (glow_img_.allocate(art::kEggImgW + 2 * kGlowPad, art::kEggImgH + 2 * kGlowPad)) {
    ui::Canvas c = glow_img_.canvas();
    const float cx = static_cast<float>(c.w) * 0.5f, top = 2.0f + kGlowPad;
    const float w = G::kEgg.w, h = G::kEgg.h;
    ui::paint(c, 0, 0, static_cast<float>(c.w), static_cast<float>(c.h), pal::kWhite, 255, [=](float x, float y) {
      return art::egg_inside(x, y, cx, top, w, h, 7.0f) && !art::egg_inside(x, y, cx, top, w, h, 3.0f);
    });
  }
  if (heart_img_.allocate(kHeartW, kHeartH)) {
    ui::Canvas c = heart_img_.canvas();
    ui::paint(c, 0, 0, kHeartW, kHeartH, pal::kPink, 255,
              [](float x, float y) { return ui::in_heart(x, y, kHeartW * 0.5f, 15.0f, kHeartSize); });
  }

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  band(parent, 0, 62, pal::kSkyDark);
  band(parent, kScreenHeight - 10, 10, pal::kGround);

  // Prompt badge: the current request as a picture; tap to hear it again.
  const Rect& br = G::kPromptBadge;
  badge_.obj = disc(parent, br.w, pal::kWhite);
  lv_obj_set_pos(badge_.obj, br.x, br.y);
  lv_obj_set_style_border_color(badge_.obj, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(badge_.obj, 2, 0);
  lv_obj_set_style_outline_color(badge_.obj, to_lv(pal::kWhite), 0);
  lv_obj_set_style_outline_width(badge_.obj, 0, 0);
  badge_q_.obj = lv_label_create(badge_.obj);
  lv_label_set_text_static(badge_q_.obj, "?");
#if LV_FONT_MONTSERRAT_28
  lv_obj_set_style_text_font(badge_q_.obj, &lv_font_montserrat_28, 0);
#endif
  lv_obj_set_style_text_color(badge_q_.obj, to_lv(pal::kInk), 0);
  lv_obj_center(badge_q_.obj);
  badge_icon_.obj = image(badge_.obj);
  lv_image_set_src(badge_icon_.obj, icons_[0].dsc());
  lv_image_set_scale(badge_icon_.obj, kBadgeIconScale);
  lv_obj_center(badge_icon_.obj);
  // "Tap the egg": a small crack on the badge egg (icon-local coordinates,
  // matching the icon scaled about its centre).
  badge_crack_.obj = crack_line(badge_icon_.obj, 2);
  {
    const float s = static_cast<float>(kBadgeIconScale) / 256.0f, icx = art::kEggIconW * 0.5f, icy = art::kEggIconH * 0.5f;
    const float ew = art::kEggIconW - 4, eh = art::kEggIconH - 4;
    const int n = art::visible_crack_points(2, 4);
    for (int i = 0; i < n; ++i) {
      const float x = icx + art::kCrack[i][0] * ew, y = 2.0f + art::kCrack[i][1] * eh;
      badge_crack_pts_[i] = {static_cast<lv_value_precise_t>(icx + (x - icx) * s),
                             static_cast<lv_value_precise_t>(icy + (y - icy) * s)};
    }
    lv_line_set_points(badge_crack_.obj, badge_crack_pts_, static_cast<uint32_t>(n));
  }
  lv_obj_t* speaker = lv_label_create(parent);
  lv_label_set_text_static(speaker, LV_SYMBOL_VOLUME_MAX);
  lv_obj_set_style_text_color(speaker, to_lv(pal::kInk), 0);
  lv_obj_set_pos(speaker, br.x + br.w + 4, br.y + br.h / 2 - 8);

  // Play objects, back to front.
  shadow_.obj = plain(parent);
  lv_obj_set_style_radius(shadow_.obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(shadow_.obj, to_lv(pal::kInk), 0);
  lv_obj_set_style_bg_opa(shadow_.obj, LV_OPA_20, 0);

  glow_.obj = image(parent);
  if (glow_img_.ready()) lv_image_set_src(glow_.obj, glow_img_.dsc());
  lv_obj_set_pos(glow_.obj, kEggImgX - kGlowPad, kEggImgY - kGlowPad);
  lv_image_set_pivot(glow_.obj, art::kEggImgW / 2 + kGlowPad, art::kEggImgH - 2 + kGlowPad);

  for (Sprite* s : {&whole_, &bottom_}) {
    s->obj = image(parent);
    lv_image_set_pivot(s->obj, art::kEggImgW / 2, art::kEggImgH - 2);  // bottom centre: wobble rocks the egg
  }
  crack_.obj = crack_line(parent, 3);
  creature_.obj = image(parent);
  lv_image_set_pivot(creature_.obj, art::kCreatureImg / 2, art::kCreatureImg / 2);
  top_.obj = image(parent);
  lv_image_set_pivot(top_.obj, art::kEggImgW / 2, 2);  // the top shell tips off around its tip

  static constexpr uint32_t kSparkleColors[] = {pal::kRed, pal::kBlue, pal::kGreen, pal::kYellow, pal::kWhite};
  for (int i = 0; i < kSparkles; ++i) {
    sparkles_[i].obj = plain(parent);
    lv_obj_set_style_bg_color(sparkles_[i].obj, to_lv(kSparkleColors[i % 5]), 0);
    lv_obj_set_style_bg_opa(sparkles_[i].obj, LV_OPA_COVER, 0);
  }
  for (Sprite& h : hearts_) {
    h.obj = image(parent);
    if (heart_img_.ready()) lv_image_set_src(h.obj, heart_img_.dsc());
    lv_image_set_scale(h.obj, 119);  // 14 px hearts
  }

  // PET choices. NO = "stay with my pet" (heart), YES = "another egg" (egg).
  yes_ring_.obj = disc(parent, 76, pal::kWhite, LV_OPA_TRANSP);
  lv_obj_set_style_border_color(yes_ring_.obj, to_lv(pal::kWhite), 0);
  lv_obj_set_style_border_width(yes_ring_.obj, 4, 0);
  no_card_.obj = card(parent, G::kNoButton);
  lv_obj_t* no_heart = image(no_card_.obj);
  if (heart_img_.ready()) lv_image_set_src(no_heart, heart_img_.dsc());
  lv_obj_align(no_heart, LV_ALIGN_CENTER, 0, 1);
  yes_card_.obj = card(parent, G::kYesButton);
  yes_icon_.obj = image(yes_card_.obj);
  lv_obj_align(yes_icon_.obj, LV_ALIGN_CENTER, 0, 0);

  // Everything starts hidden; sync() shows what the phase needs.
  for (Sprite* s : {&shadow_, &glow_, &whole_, &crack_, &bottom_, &creature_, &top_, &yes_ring_, &no_card_, &yes_card_,
                    &badge_q_, &badge_icon_}) {
    s->visible = true;
    s->show(false);
  }
  for (Sprite& s : sparkles_) {
    s.visible = true;
    s.show(false);
  }
  for (Sprite& s : hearts_) {
    s.visible = true;
    s.show(false);
  }
  initialised_ = false;
}

// ---------------------------------------------------------------- sync

void HatchAPetView::sync(const HatchAPetGame& game, uint32_t now_ms) {
  const Phase ph = game.phase();
  if (!initialised_ || ph != last_phase_) {
    if (!initialised_) {
      last_cracks_ = game.cracks();
      last_pets_ = game.pet_count();
      wobble_armed_ = false;
      react_armed_ = false;
    }
    last_phase_ = ph;
    phase_start_ms_ = now_ms;
    if (ph == Phase::PET) {  // the new pet greets the child with a hop
      react_start_ms_ = now_ms;
      react_armed_ = true;
    }
  }
  initialised_ = true;
  if (game.cracks() != last_cracks_) {
    if (game.cracks() > last_cracks_) {
      wobble_start_ms_ = now_ms;
      wobble_armed_ = true;
    }
    last_cracks_ = game.cracks();
  }
  if (game.pet_count() != last_pets_) {
    last_pets_ = game.pet_count();
    react_start_ms_ = now_ms;
    react_armed_ = true;
  }

  sync_badge(game, now_ms);
  switch (ph) {
    case Phase::INTRO: sync_intro(game, now_ms); break;
    case Phase::EGG: sync_egg(game, now_ms); break;
    case Phase::HATCH: sync_hatch(game, now_ms); break;
    case Phase::PET: sync_pet(game, now_ms); break;
  }
}

void HatchAPetView::sync_badge(const HatchAPetGame& game, uint32_t now_ms) {
  const Phase ph = game.phase();
  const int pattern = static_cast<int>(game.egg_pattern());
  // -1 = "?", 0..3 = "tap this egg", 4..7 = "another egg?" (YES picture).
  const int state = ph == Phase::INTRO ? -1 : (ph == Phase::PET ? 4 + (pattern + 1) % 4 : pattern);
  if (state != badge_state_) {
    badge_state_ = state;
    badge_q_.show(state < 0);
    badge_icon_.show(state >= 0);
    badge_crack_.show(state >= 0 && state < 4);
    if (state >= 0) badge_icon_.source(icons_[state % 4].dsc());
  }
  const int32_t pad = game.hint_active() ? 3 + round_i(2.0f * pulse(now_ms)) : 0;
  if (pad != badge_pad_) {
    badge_pad_ = pad;
    lv_obj_set_style_outline_width(badge_.obj, pad ? 3 : 0, 0);
    lv_obj_set_style_outline_pad(badge_.obj, pad, 0);
  }
}

void HatchAPetView::place_shadow(int32_t cx, int32_t cy, int32_t rx) {
  shadow_.show(true);
  shadow_.size(2 * rx, 14);
  shadow_.pos(cx - rx, cy - 7);
}

// Whole egg (plus crack line) rocked by `rot` radians about its bottom centre.
void HatchAPetView::place_whole_egg(EggPattern p, int32_t dy, float rot, int crack_points) {
  const lv_image_dsc_t* src = egg(p, static_cast<int>(art::Piece::WHOLE), true);
  whole_.show(src != nullptr);
  if (src) {
    whole_.source(src);
    whole_.pos(kEggImgX, kEggImgY + dy);
    whole_.rotation(round_i(rot * kRadToDeciDeg));
  }
  if (crack_points < 2) {
    crack_.show(false);
    return;
  }
  if (crack_points != crack_n_ || rot != crack_rot_) {
    crack_n_ = crack_points;
    crack_rot_ = rot;
    const float c = std::cos(rot), s = std::sin(rot), px = kEggCx, py = static_cast<float>(kEggBottom + dy);
    for (int i = 0; i < crack_points; ++i) {
      const float x = kEggCx + art::kCrack[i][0] * G::kEgg.w - px;
      const float y = G::kEgg.y + dy + art::kCrack[i][1] * G::kEgg.h - py;
      crack_pts_[i] = {static_cast<lv_value_precise_t>(px + x * c - y * s), static_cast<lv_value_precise_t>(py + x * s + y * c)};
    }
    lv_line_set_points_mutable(crack_.obj, crack_pts_, static_cast<uint32_t>(crack_points));
  }
  crack_.show(true);
}

void HatchAPetView::hide_play() {
  for (Sprite* s : {&bottom_, &creature_, &top_, &yes_ring_, &no_card_, &yes_card_}) s->show(false);
  for (Sprite& s : sparkles_) s.show(false);
  for (Sprite& s : hearts_) s.show(false);
}

void HatchAPetView::sync_intro(const HatchAPetGame& game, uint32_t now_ms) {
  hide_play();
  glow_.show(false);
  // The egg drops in with a soft bounce.
  const float t = clamp01(static_cast<float>(now_ms - phase_start_ms_) / 600.0f);
  const float drop = (1.0f - t) * (1.0f - t) * -60.0f + std::sin(t * kPi) * -6.0f;
  place_shadow(kEggCx + 3, kEggBottom - 2, 47);
  place_whole_egg(game.egg_pattern(), round_i(drop), 0.0f, 0);
  if (t >= 1.0f) prewarm(game);
}

void HatchAPetView::sync_egg(const HatchAPetGame& game, uint32_t now_ms) {
  hide_play();
  float wob = 0.0f;
  if (wobble_armed_) {
    const float k = 1.0f - static_cast<float>(now_ms - wobble_start_ms_) / static_cast<float>(G::kWobbleMs);
    if (k > 0.0f) {
      wob = std::sin((1.0f - k) * kPi * 4.0f) * 0.14f * k;
    } else {
      wobble_armed_ = false;
    }
  }
  place_shadow(kEggCx + 3, kEggBottom - 2, 47);
  place_whole_egg(game.egg_pattern(), 0, wob, art::visible_crack_points(game.cracks(), game.taps_needed()));
  if (game.hint_active() && glow_img_.ready()) {
    glow_.show(true);
    glow_.rotation(round_i(wob * kRadToDeciDeg));
    glow_.image_opa(round_i(255.0f * (0.6f + 0.4f * pulse(now_ms))));
  } else {
    glow_.show(false);
  }
  if (!wobble_armed_) prewarm(game);  // paint while nothing moves
}

void HatchAPetView::sync_hatch(const HatchAPetGame& game, uint32_t now_ms) {
  glow_.show(false);
  for (Sprite* s : {&yes_ring_, &no_card_, &yes_card_}) s->show(false);
  for (Sprite& s : hearts_) s.show(false);
  const float t = clamp01(static_cast<float>(now_ms - phase_start_ms_) / static_cast<float>(G::kHatchMs));
  if (t < 0.2f) {  // excited shake
    for (Sprite* s : {&bottom_, &creature_, &top_}) s->show(false);
    for (Sprite& s : sparkles_) s.show(false);
    place_shadow(kEggCx + 3, kEggBottom - 2, 47);
    place_whole_egg(game.egg_pattern(), 0, std::sin(t * 60.0f) * 0.06f, art::kCrackPoints);
    prewarm(game);
    return;
  }
  shadow_.show(false);
  whole_.show(false);
  crack_.show(false);
  const float u = clamp01((t - 0.2f) / 0.3f), grow = clamp01((t - 0.2f) / 0.35f);
  const float bounce = grow < 1.0f ? 0.4f + 0.6f * grow : 1.0f + std::sin(clamp01((t - 0.55f) / 0.3f) * kPi) * 0.08f;
  const float fade_top = clamp01(1.0f - (t - 0.2f) / 0.35f);
  const float fade_bottom = t < 0.5f ? 1.0f : clamp01(1.0f - (t - 0.5f) / 0.3f);

  const EggPattern p = game.egg_pattern();
  const lv_image_dsc_t* bsrc = fade_bottom > 0.0f ? egg(p, static_cast<int>(art::Piece::BOTTOM), true) : nullptr;
  bottom_.show(bsrc != nullptr);
  if (bsrc) {
    bottom_.source(bsrc);
    bottom_.pos(kEggImgX, kEggImgY + round_i(u * 18.0f));
    bottom_.image_opa(round_i(255.0f * fade_bottom));
  }
  const lv_image_dsc_t* csrc = creature(game.creature(), true);
  creature_.show(csrc != nullptr);
  if (csrc) {
    creature_.source(csrc);
    creature_.pos(kCreatureCx - art::kCreatureImg / 2, kCreatureCy - art::kCreatureImg / 2 + round_i((1.0f - grow) * 30.0f));
    const int32_t sc = round_i(256.0f * bounce);
    creature_.scale(sc, sc);
  }
  const lv_image_dsc_t* tsrc = fade_top > 0.0f ? egg(p, static_cast<int>(art::Piece::TOP), true) : nullptr;
  top_.show(tsrc != nullptr);
  if (tsrc) {
    top_.source(tsrc);
    top_.pos(kEggImgX - round_i(u * 70.0f), kEggImgY - round_i(u * 6.0f));
    top_.rotation(round_i(-u * 0.9f * kRadToDeciDeg));
    top_.image_opa(round_i(255.0f * fade_top));
  }
  for (Sprite& s : sparkles_) s.show(t > 0.3f);
  if (t > 0.3f) {
    const float k = clamp01((t - 0.3f) / 0.6f), d = 20.0f + k * 60.0f;
    const int32_t sz = round_i(6.0f * (1.0f - k)) + 1;
    for (int i = 0; i < kSparkles; ++i) {
      const float a = static_cast<float>(i) * 2.0f * kPi / kSparkles;
      sparkles_[i].size(sz, sz);
      sparkles_[i].pos(kCreatureCx + round_i(std::cos(a) * d) - sz / 2, kCreatureCy + round_i(std::sin(a) * d) - sz / 2);
    }
  }
}

void HatchAPetView::sync_pet(const HatchAPetGame& game, uint32_t now_ms) {
  shadow_.show(false);
  whole_.show(false);
  crack_.show(false);
  glow_.show(false);
  bottom_.show(false);
  top_.show(false);
  for (Sprite& s : sparkles_) s.show(false);

  float r = 0.0f;
  if (react_armed_) {
    r = static_cast<float>(now_ms - react_start_ms_) / static_cast<float>(G::kReactMs);
    if (r >= 1.0f) {
      react_armed_ = false;
      r = 0.0f;
    }
  }
  const float hop = r > 0.0f ? -std::sin(r * kPi) * 14.0f : 0.0f;
  const float squash = r > 0.0f ? 1.0f + std::sin(r * kPi * 2.0f) * 0.05f : 1.0f;
  place_shadow(kCreatureCx + 3, G::kCreature.y + G::kCreature.h + 2,
               46 - (r > 0.0f ? round_i(std::sin(r * kPi) * 10.0f) : 0));
  if (const lv_image_dsc_t* src = creature(game.creature(), true)) {
    creature_.show(true);
    creature_.source(src);
    creature_.pos(kCreatureCx - art::kCreatureImg / 2, kCreatureCy - art::kCreatureImg / 2 + round_i(hop));
    creature_.scale(round_i(256.0f / squash), round_i(256.0f * squash));
  }
  for (int i = 0; i < kHearts; ++i) {
    const float k = r > 0.0f ? std::fmin(1.0f, r * 1.3f - static_cast<float>(i) * 0.15f) : 0.0f;
    Sprite& h = hearts_[i];
    h.show(k > 0.0f && heart_img_.ready());
    if (k <= 0.0f) continue;
    h.pos(kCreatureCx - 40 + i * 40 - kHeartW / 2, G::kCreature.y + 10 - round_i(k * 34.0f) - 15);
    h.image_opa(round_i(255.0f * (1.0f - k)));
  }

  no_card_.show(true);
  yes_card_.show(true);
  yes_icon_.source(icons_[(static_cast<std::size_t>(game.egg_pattern()) + 1) % kEggPatternCount].dsc());
  yes_ring_.show(game.hint_active());
  if (game.hint_active()) {
    const int32_t d = 76 + round_i(4.0f * pulse(now_ms));
    yes_ring_.size(d, d);
    yes_ring_.pos(G::kYesButton.x + G::kYesButton.w / 2 - d / 2, G::kYesButton.y + G::kYesButton.h / 2 - d / 2);
  }
}

}  // namespace little_arcade::hatch_a_pet
