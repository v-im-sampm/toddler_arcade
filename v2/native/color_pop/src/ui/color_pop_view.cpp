#include "ui/color_pop_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace color_pop {
namespace {
constexpr double kPi = 3.141592653589793, kTau = 2 * kPi;

int32_t iround(double v) { return static_cast<int32_t>(std::floor(v + 0.5)); }
int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
lv_color_t hex(uint32_t rgb) { return lv_color_hex(rgb & 0xFFFFFF); }
lv_opa_t to_opa(double a) { return static_cast<lv_opa_t>(clampi(iround(clampd(a, 0, 1) * 255), 0, 255)); }

// CSS hsl() -> RGB (rim colour hsl((time*60 + ph*40) % 360, 90%, 80%)).
uint32_t hsl(double h, double s, double l) {
  h = std::fmod(h, 360.0);
  if (h < 0) h += 360;
  const double a = s * std::min(l, 1 - l);
  auto f = [&](double n) {
    const double k = std::fmod(n + h / 30, 12.0);
    return l - a * std::max(-1.0, std::min({k - 3, 9 - k, 1.0}));
  };
  const auto c = [](double v) { return static_cast<uint32_t>(clampi(iround(v * 255), 0, 255)); };
  return (c(f(0)) << 16) | (c(f(8)) << 8) | c(f(4));
}
// Positive remainder.
double wrap(double v, double m) { v = std::fmod(v, m); return v < 0 ? v + m : v; }
}  // namespace

void View::Box::add(int32_t ax1, int32_t ay1, int32_t ax2, int32_t ay2) {
  if (x2 < x1) { x1 = ax1; y1 = ay1; x2 = ax2; y2 = ay2; return; }
  x1 = std::min(x1, ax1); y1 = std::min(y1, ay1); x2 = std::max(x2, ax2); y2 = std::max(y2, ay2);
}

const Sprite* View::need(const char* name) {
  const Sprite* s = pack_->find(name);
  if (!s && !missing_) missing_ = name;
  return s;
}

View::Layer View::make(lv_obj_t* parent, const Sprite* sprite, bool mask) {
  Layer l;
  l.obj = lv_image_create(parent);
  lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_SCROLLABLE);
  if (mask) lv_obj_set_style_image_recolor_opa(l.obj, LV_OPA_COVER, 0);
  if (sprite) {
    lv_image_set_src(l.obj, &sprite->dsc);
    lv_image_set_pivot(l.obj, iround(sprite->ox), iround(sprite->oy));
    l.sprite = sprite;
  }
  hide(l);
  return l;
}

void View::hide(Layer& l) {
  if (!l.hidden) { lv_obj_add_flag(l.obj, LV_OBJ_FLAG_HIDDEN); l.hidden = true; }
}

void View::tint(Layer& l, uint32_t rgb) {
  if (l.color != rgb) { lv_obj_set_style_image_recolor(l.obj, hex(rgb), 0); l.color = rgb; }
}

// Draws `sprite` so that its reference origin lands at (x, y), rotated by `rot` radians and
// scaled about that origin (Canvas translate -> rotate -> scale == LVGL scale then rotate).
void View::place(Layer& l, const Sprite* sprite, double x, double y, double rot, double sx, double sy, double opa) {
  if (!sprite || opa <= 0) { hide(l); return; }
  if (l.sprite != sprite) {
    lv_image_set_src(l.obj, &sprite->dsc);
    lv_image_set_pivot(l.obj, iround(sprite->ox), iround(sprite->oy));
    l.sprite = sprite;
  }
  const int32_t px = iround(x - sprite->ox), py = iround(y - sprite->oy);
  if (px != l.x || py != l.y) { lv_obj_set_pos(l.obj, px, py); l.x = px; l.y = py; }
  int32_t r = iround(rot * 1800 / kPi) % 3600;
  if (r < 0) r += 3600;
  if (r != l.rot) { lv_image_set_rotation(l.obj, r); l.rot = r; }
  const uint32_t isx = static_cast<uint32_t>(std::max<int32_t>(1, iround(sx * 256)));
  const uint32_t isy = static_cast<uint32_t>(std::max<int32_t>(1, iround(sy * 256)));
  if (isx != l.sx) { lv_image_set_scale_x(l.obj, isx); l.sx = isx; }
  if (isy != l.sy) { lv_image_set_scale_y(l.obj, isy); l.sy = isy; }
  const lv_opa_t o = to_opa(opa);
  if (o != l.opa) { lv_obj_set_style_image_opa(l.obj, o, 0); l.opa = o; }
  if (l.hidden) { lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_HIDDEN); l.hidden = false; }
}

bool View::create(lv_obj_t* screen, const AssetPack& pack, const char** missing) {
  pack_ = &pack;
  missing_ = nullptr;
  char name[32];
  for (int k = 0; k < 16; ++k) { std::snprintf(name, sizeof name, "shimmer_%d", k); shimmer_frames_[k] = need(name); }
  static const char* kColorNames[4] = {"red", "yellow", "green", "blue"};  // Color enum order
  for (int c = 0; c < 4; ++c) {
    std::snprintf(name, sizeof name, "bubble_%s", kColorNames[c]); bubble_body_[c] = need(name);
    std::snprintf(name, sizeof name, "badge_%s_glyph", kColorNames[c]); badge_frames_[c][0] = need(name);
    std::snprintf(name, sizeof name, "badge_%s_happy", kColorNames[c]); badge_frames_[c][1] = need(name);
    std::snprintf(name, sizeof name, "bubble_glow_%d", c + 1); bubble_glow_[c] = need(name);
  }
  bubble_rim_ = need("bubble_rim");
  for (int k = 0; k < 8; ++k) { std::snprintf(name, sizeof name, "crab_legs_%d", k); crab_legs_[k] = need(name); }
  for (int k = 0; k < 6; ++k) {
    std::snprintf(name, sizeof name, "crab_arm_l_%d", k); crab_arm_l_[k] = need(name);
    std::snprintf(name, sizeof name, "crab_arm_r_%d", k); crab_arm_r_[k] = need(name);
  }
  for (int k = 0; k < 29; ++k) { std::snprintf(name, sizeof name, "crab_point_%d", k); crab_point_[k] = need(name); }
  crab_body_[0][0] = need("crab_body_open_calm");
  crab_body_[0][1] = need("crab_body_open_happy");
  crab_body_[1][0] = need("crab_body_blink_calm");
  crab_body_[1][1] = need("crab_body_blink_happy");
  for (int k = 0; k < 18; ++k) { std::snprintf(name, sizeof name, "decor_%d", k); decor_[k] = need(name); }
  for (int k = 0; k < 13; ++k) { std::snprintf(name, sizeof name, "pcircle_%d", k); pcircle_[k] = need(name); }
  for (int s = 0; s < 7; ++s)
    for (int a = 0; a < 6; ++a) { std::snprintf(name, sizeof name, "pstar_%d_%d", s, a); pstar_[s][a] = need(name); }
  for (int s = 0; s < 4; ++s)
    for (int a = 0; a < 12; ++a) { std::snprintf(name, sizeof name, "prect_%d_%d", s, a); prect_[s][a] = need(name); }
  const Sprite* veil = need("start_veil");
  const Sprite* bg = need("bg");
  const Sprite* cloud = need("cloud");
  const Sprite* pupil = need("crab_pupil");
  const Sprite* hand = need("hand");
  const Sprite* badge_glow = need("badge_glow");
  const Sprite* home = need("chrome_home");
  const Sprite* mic = need("chrome_mic");
  const Sprite* start_button = need("start_button");
  if (missing_) { if (missing) *missing = missing_; return false; }

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

  // Reference frame(): background, bubbles, crab, helper hand, FX, badge, chrome, start overlay.
  bg_ = make(screen, bg, false);
  place(bg_, bg, 0, 0);
  for (Layer& c : cloud_) { c = make(screen, cloud, true); tint(c, 0xFFFFFF); }
  shimmer_ = make(screen, shimmer_frames_[0], true);
  tint(shimmer_, 0xFFFFFF);
  for (BubbleLayers& b : bubbles_) {
    b.glow = make(screen, bubble_glow_[0], true);
    tint(b.glow, 0xFFE066);  // TOK.hintGlow
    b.body = make(screen, bubble_body_[0], false);
    b.rim = make(screen, bubble_rim_, true);
  }
  crab_.legs = make(screen, crab_legs_[0], true);
  tint(crab_.legs, 0xC2621B);
  crab_.arm_l = make(screen, crab_arm_l_[0], false);
  crab_.arm_r = make(screen, crab_arm_r_[0], false);
  crab_.body = make(screen, crab_body_[0][0], false);
  crab_.pupil_l = make(screen, pupil, true);
  crab_.pupil_r = make(screen, pupil, true);
  tint(crab_.pupil_l, 0x3B3550);
  tint(crab_.pupil_r, 0x3B3550);
  hand_ = make(screen, hand, false);

  fx_ = lv_obj_create(screen);
  lv_obj_remove_style_all(fx_);
  lv_obj_remove_flag(fx_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(fx_, kWidth, kHeight);
  lv_obj_add_event_cb(fx_, fx_event, LV_EVENT_DRAW_MAIN, this);

  badge_glow_ = make(screen, badge_glow, true);
  tint(badge_glow_, 0xFFE066);
  badge_ = make(screen, badge_frames_[0][0], false);
  home_ = make(screen, home, false);
  mic_ = make(screen, mic, false);
  place(home_, home, 4, 4);
  place(mic_, mic, 268, 4);

  // drawStart() veil is a flat rgba fill; take its exact colour from the exported pixel.
  const uint8_t* px = static_cast<const uint8_t*>(veil->dsc.data);
  const uint16_t c565 = static_cast<uint16_t>(px[0] | (px[1] << 8));
  veil_ = lv_obj_create(screen);
  lv_obj_remove_style_all(veil_);
  lv_obj_remove_flag(veil_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(veil_, kWidth, kHeight);
  lv_obj_set_style_bg_color(veil_, lv_color_make(static_cast<uint8_t>((c565 >> 11) * 255 / 31),
                                                 static_cast<uint8_t>(((c565 >> 5) & 63) * 255 / 63),
                                                 static_cast<uint8_t>((c565 & 31) * 255 / 31)), 0);
  lv_obj_set_style_bg_opa(veil_, px[2], 0);
  start_button_ = make(screen, start_button, false);
  return true;
}

void View::sync(const Game& g) {
  game_ = &g;
  const double time = g.time();
  const bool reduced = g.reduced();

  // drawBackground(): two drifting clouds and the horizon shimmer.
  static constexpr double kClouds[2][3] = {{40, 22, 4}, {210, 38, 2.6}};
  for (int i = 0; i < 2; ++i) {
    const double x = std::fmod(kClouds[i][0] + time * kClouds[i][2] * (reduced ? 0.3 : 1), kWidth + 80.0) - 40;
    place(cloud_[i], cloud_[i].sprite, x, kClouds[i][1]);
  }
  const double period = kTau / 1.2;
  const int sf = static_cast<int>(std::floor(wrap(time, period) / period * 16 + 0.5)) % 16;
  place(shimmer_, shimmer_frames_[sf], 0, 62);

  // drawBubble() for every bubble with t >= 0 (pool order == bubbles order == draw order).
  const auto& bubbles = g.bubbles();
  for (std::size_t i = 0; i < 6; ++i) {
    BubbleLayers& L = bubbles_[i];
    if (i >= bubbles.size() || bubbles[i].t < 0) { hide(L.glow); hide(L.body); hide(L.rim); continue; }
    const Bubble& b = bubbles[i];
    const double sx = b.sx * b.s, sy = b.sy * b.s;
    if (b.glow > 0) {
      const int k = clampi(iround(b.glow * 4), 1, 4) - 1;
      place(L.glow, bubble_glow_[k], b.x, b.y, b.rot, sx, sy, b.alpha * (0.35 + 0.5 * b.glow));
    } else {
      hide(L.glow);
    }
    place(L.body, bubble_body_[static_cast<int>(b.color)], b.x, b.y, b.rot, sx, sy, b.alpha);
    tint(L.rim, hsl(std::fmod(time * 60 + b.ph * 40, 360.0), 0.9, 0.8));
    place(L.rim, bubble_rim_, b.x, b.y, b.rot + b.ph * 0.3, sx, sy, b.alpha * 0.35);
  }

  // drawCrab(): one shared transform for all parts.
  const Crab& c = g.crab();
  const double jump = c.clap > 0 ? -std::sin((1 - c.clap / 0.6) * kPi) * 8 : 0;
  const double dance = c.dance > 0 ? std::sin(c.dance * 10) : 0;
  const double ox = 42, oy = 222 + c.y_off + jump - std::fabs(dance) * 6;
  const double rot = (c.giggle > 0 ? std::sin(c.giggle * 40) * 0.12 : 0) + dance * 0.15;
  const double br = std::sin(time * kTau / 2.8) * 0.025;
  const double csx = 1 + br, csy = 1 - br;
  const int leg = static_cast<int>(std::floor(wrap(time * 6, kTau) / kTau * 8 + 0.5)) % 8;
  place(crab_.legs, crab_legs_[leg], ox, oy, rot, csx, csy);
  const double clap_t = c.clap > 0 ? std::fabs(std::sin((1 - c.clap / 0.6) * kPi * 3)) : 0;
  const int ck = clampi(iround(clap_t * 5), 0, 5);
  place(crab_.arm_l, crab_arm_l_[ck], ox, oy, rot, csx, csy);
  if (c.pointing) {
    const double deg = std::atan2(c.point_y - 222, c.point_x - 42) * 180 / kPi;
    place(crab_.arm_r, crab_point_[clampi(iround((deg + 80) / 5), 0, 28)], ox, oy, rot, csx, csy);
  } else {
    place(crab_.arm_r, crab_arm_r_[ck], ox, oy, rot, csx, csy);
  }
  const bool blink = c.blink < 0;
  const bool happy = c.clap > 0 || c.dance > 0 || c.giggle > 0;
  place(crab_.body, crab_body_[blink ? 1 : 0][happy ? 1 : 0], ox, oy, rot, csx, csy);
  if (blink) {
    hide(crab_.pupil_l); hide(crab_.pupil_r);
  } else {
    const double ln = std::hypot(c.look_x, c.look_y) > 0 ? std::hypot(c.look_x, c.look_y) : 1;
    const double lx = c.look_x / ln * 1.8, ly = c.look_y / ln * 1.8;
    const double cr = std::cos(rot), sr = std::sin(rot);
    Layer* pupils[2] = {&crab_.pupil_l, &crab_.pupil_r};
    for (int s = 0; s < 2; ++s) {
      const double px = ((s ? 8 : -8) + lx) * csx, py = (-33 + ly) * csy;
      place(*pupils[s], pupils[s]->sprite, ox + px * cr - py * sr, oy + px * sr + py * cr);
    }
  }

  // drawHelperHand()
  const int hb = g.helper_bubble();
  if (hb >= 0) {
    const Bubble& b = bubbles[static_cast<std::size_t>(hb)];
    place(hand_, hand_.sprite, b.x + 14, b.y + 30 - std::fabs(std::sin(time * 4)) * 6);
  } else {
    hide(hand_);
  }

  // drawFx(): invalidate the tiles touched by FX last frame or this frame.
  Tiles now{};
  fx_tiles(g, now);
  invalidate_fx(now);
  std::memcpy(fx_prev_, now, sizeof(Tiles));

  // drawBadge()
  const Phase phase = g.phase();
  if (g.has_target() && phase != Phase::Start && phase != Phase::Intro) {
    const Badge& bd = g.badge();
    const double bob = std::sin(time * 2.2) * 1.5 * (reduced ? 0.3 : 1);
    const double press = 1 + clampd(-bd.sv * 0.012, -0.1, 0.12);
    if (g.hint() >= 1 && phase == Phase::Play) {
      place(badge_glow_, badge_glow_.sprite, 160, 32 + bob, 0, press, 2 - press,
            0.4 + 0.4 * std::sin(time * kTau / 0.9));
    } else {
      hide(badge_glow_);
    }
    place(badge_, badge_frames_[static_cast<int>(g.target())][bd.happy > 0 ? 1 : 0], 160, 32 + bob, 0, press, 2 - press);
  } else {
    hide(badge_glow_);
    hide(badge_);
  }

  // drawStart()
  if (phase == Phase::Start) {
    lv_obj_remove_flag(veil_, LV_OBJ_FLAG_HIDDEN);
    const double p = 1 + std::sin(time * 3) * 0.04;
    place(start_button_, start_button_.sprite, 160, 120, 0, p, p);
  } else {
    lv_obj_add_flag(veil_, LV_OBJ_FLAG_HIDDEN);
    hide(start_button_);
  }
}

void View::fx_tiles(const Game& g, Tiles& tiles) const {
  auto mark = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    const int tx1 = clampi(x1 / kTile, 0, kTilesX - 1), tx2 = clampi(x2 / kTile, 0, kTilesX - 1);
    const int ty1 = clampi(y1 / kTile, 0, kTilesY - 1), ty2 = clampi(y2 / kTile, 0, kTilesY - 1);
    if (x2 < 0 || y2 < 0 || x1 >= kWidth || y1 >= kHeight) return;
    for (int ty = ty1; ty <= ty2; ++ty)
      for (int tx = tx1; tx <= tx2; ++tx) tiles[ty][tx] = 1;
  };
  for (const Decor& d : g.decor()) {
    const int32_t r = static_cast<int32_t>(std::ceil(d.r)) + 2;
    mark(iround(d.x) - r, iround(d.y) - r, iround(d.x) + r, iround(d.y) + r);
  }
  for (const Ring& r : g.rings()) {
    const int32_t m = static_cast<int32_t>(std::ceil(std::max(r.r0, r.r1) + r.w)) + 2;
    mark(iround(r.x) - m, iround(r.y) - m, iround(r.x) + m, iround(r.y) + m);
  }
  for (const Particle& p : g.particles()) {
    const int32_t m = 10;
    mark(iround(p.x) - m, iround(p.y) - m, iround(p.x) + m, iround(p.y) + m);
  }
}

void View::invalidate_fx(const Tiles& now) {
  // Horizontal runs per tile row, merged downwards while the run is identical. LVGL keeps at
  // most LV_INV_BUF_SIZE (32) areas and then redraws the whole screen, so fall back to the
  // union box when the tiles fragment into too many rectangles.
  constexpr int kMaxRects = 20;
  lv_area_t rects[kMaxRects];
  int n = 0;
  bool overflow = false;
  Box all;
  int open[kTilesX];  // index of the rect that ends at the previous row for a run starting at x
  for (int& o : open) o = -1;
  for (int ty = 0; ty < kTilesY && !overflow; ++ty) {
    int next_open[kTilesX];
    for (int& o : next_open) o = -1;
    for (int tx = 0; tx < kTilesX;) {
      if (!(now[ty][tx] | fx_prev_[ty][tx])) { ++tx; continue; }
      int end = tx;
      while (end + 1 < kTilesX && (now[ty][end + 1] | fx_prev_[ty][end + 1])) ++end;
      const int32_t x1 = tx * kTile, x2 = (end + 1) * kTile - 1, y1 = ty * kTile, y2 = (ty + 1) * kTile - 1;
      all.add(x1, y1, x2, y2);
      const int prev = open[tx];
      if (prev >= 0 && rects[prev].x2 == x2) {
        rects[prev].y2 = y2;
        next_open[tx] = prev;
      } else if (n < kMaxRects) {
        rects[n] = {x1, y1, x2, y2};
        next_open[tx] = n++;
      } else {
        overflow = true;
        break;
      }
      tx = end + 1;
    }
    std::memcpy(open, next_open, sizeof(open));
  }
  if (overflow) {
    for (int ty = 0; ty < kTilesY; ++ty)
      for (int tx = 0; tx < kTilesX; ++tx)
        if (now[ty][tx] | fx_prev_[ty][tx]) all.add(tx * kTile, ty * kTile, (tx + 1) * kTile - 1, (ty + 1) * kTile - 1);
    lv_area_t a{all.x1, all.y1, all.x2, all.y2};
    lv_obj_invalidate_area(fx_, &a);
    return;
  }
  for (int i = 0; i < n; ++i) lv_obj_invalidate_area(fx_, &rects[i]);
}

void View::fx_event(lv_event_t* e) {
  auto* self = static_cast<View*>(lv_event_get_user_data(e));
  if (self->game_) self->draw_fx(lv_event_get_layer(e));
}

void View::draw_fx(lv_layer_t* layer) {
  const Game& g = *game_;
  // LVGL calls this once per invalidated area and draw-buffer stripe; only items touching the
  // current clip area may create draw tasks (each one is a heap allocation plus dispatch).
  const lv_area_t clip = layer->_clip_area;
  auto visible = [&](const lv_area_t& a) {
    return a.x2 >= clip.x1 && a.x1 <= clip.x2 && a.y2 >= clip.y1 && a.y1 <= clip.y2;
  };
  auto blit = [&](const Sprite* s, double x, double y, uint32_t rgb, double opa, bool additive) {
    if (!s || opa <= 0) return;
    const int32_t x1 = iround(x - s->ox), y1 = iround(y - s->oy);
    lv_area_t a{x1, y1, x1 + static_cast<int32_t>(s->dsc.header.w) - 1, y1 + static_cast<int32_t>(s->dsc.header.h) - 1};
    if (!visible(a)) return;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = &s->dsc;
    d.recolor = hex(rgb);
    d.recolor_opa = LV_OPA_COVER;
    d.opa = to_opa(opa);
    if (additive) d.blend_mode = LV_BLEND_MODE_ADDITIVE;
    lv_draw_image(layer, &d, &a);
  };
  for (const Decor& d : g.decor())
    blit(decor_[clampi(iround((d.r - 1.5) / 0.5), 0, 17)], d.x, d.y, 0xFFFFFF, 1, false);
  for (const Ring& r : g.rings()) {
    const double k = r.t / r.dur;
    const double radius = r.r0 + (r.r1 - r.r0) * (1 - std::pow(1 - k, 3));
    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = hex(r.color);
    a.opa = to_opa(1 - k);
    a.width = std::max<int32_t>(1, iround(r.w));
    a.center.x = iround(r.x);
    a.center.y = iround(r.y);
    a.radius = static_cast<uint16_t>(std::max<int32_t>(1, iround(radius + r.w / 2)));
    a.start_angle = 0;
    a.end_angle = 360;
    const lv_area_t box{a.center.x - a.radius, a.center.y - a.radius, a.center.x + a.radius, a.center.y + a.radius};
    if (visible(box)) lv_draw_arc(layer, &a);
  }
  for (const Particle& p : g.particles()) {
    const double opa = clampd(p.life / p.max * 1.6, 0, 1);
    const Sprite* s = nullptr;
    switch (p.shape) {
      case Shape::Circle: {
        const double r = p.size * (0.5 + 0.5 * p.life / p.max);
        s = pcircle_[clampi(iround((r - 1) / 0.5), 0, 12)];
        break;
      }
      case Shape::Star: {
        const int a = static_cast<int>(std::floor(wrap(p.rot, kTau / 5) / (kTau / 5) * 6 + 0.5)) % 6;
        s = pstar_[clampi(iround((p.size - 3.5) / 0.5), 0, 6)][a];
        break;
      }
      case Shape::Rect: {
        const int a = static_cast<int>(std::floor(wrap(p.rot, kPi) / kPi * 12 + 0.5)) % 12;
        s = prect_[clampi(iround(p.size - 4.5), 0, 3)][a];
        break;
      }
    }
    blit(s, p.x, p.y, p.color, opa, p.additive);
  }
}

}  // namespace color_pop
