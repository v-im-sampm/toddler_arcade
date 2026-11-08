#include "ui/sound_jar_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sound_jar {
namespace {
constexpr double kPi = 3.141592653589793, kTau = 2 * kPi;
constexpr double kFootX = 160, kFootY = 156;  // jar transform origin: translate(JAR.x, LEDGE - 1)
constexpr double kBodyDy = kJarY - kFootY;    // -36: jar body centre relative to the foot
constexpr double kJarA = 44, kJarB = 36, kJarN = 2.6;
constexpr uint32_t kInkSoft = 0x6F6888;

int32_t iround(double v) { return static_cast<int32_t>(std::floor(v + 0.5)); }
int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
lv_color_t hex(uint32_t rgb) { return lv_color_hex(rgb & 0xFFFFFF); }
lv_opa_t to_opa(double a) { return static_cast<lv_opa_t>(clampi(iround(clampd(a, 0, 1) * 255), 0, 255)); }
double wrap(double v, double m) { v = std::fmod(v, m); return v < 0 ? v + m : v; }
// Nearest of `n` frames covering one `period` of a cyclic value.
int cyc(double v, double period, int n) { return static_cast<int>(std::floor(wrap(v, period) / period * n + 0.5)) % n; }
double ease_in_out(double t) { return t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2; }
double ease_out_back(double t) { const double c1 = 1.70158, c3 = c1 + 1; return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2); }
double ease_out_cubic(double t) { return 1 - std::pow(1 - t, 3); }
double sgn(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

// Reference wiggle(o): a 360 ms, 6 degree shake.
double wiggle(const Body& o, bool reduced) {
  if (o.wig <= 0) return 0;
  const double k = o.wig / 0.36;
  return std::sin((1 - k) * kPi * 4) * 6 * kPi / 180 * k * (reduced ? 0.3 : 1);
}
// Reference jarXY(t, a, b) x coordinate.
double jar_x(double t, double a, double b) {
  const double c = std::cos(t), s = std::sin(t);
  const double y = b * sgn(s) * std::pow(std::fabs(s), 2 / kJarN);
  double x = a * sgn(c) * std::pow(std::fabs(c), 2 / kJarN);
  if (y < 0) x *= 1 - 0.1 * (-y / b);
  return x;
}
// Reference mistPath() surface geometry: ys (body coords) and half-width w.
void mist_geom(double level, double& ys, double& w) {
  const double a = kJarA - 4, b = kJarB - 4;
  ys = b - level * 2 * b;
  const double t0 = std::asin(clampd(sgn(ys) * std::pow(std::fabs(ys / b), kJarN / 2), -1, 1));
  w = jar_x(t0, a, b);
}
// Ribbon bezier (drawRibbon P0, C, P1).
constexpr double kP0x = 248, kP0y = 108, kCx = 226, kCy = 52, kP1x = 166, kP1y = 78;
void ribbon_pt(double u, double& x, double& y) {
  const double a = (1 - u) * (1 - u), b = 2 * u * (1 - u), c = u * u;
  x = a * kP0x + b * kCx + c * kP1x;
  y = a * kP0y + b * kCy + c * kP1y;
}
}  // namespace

void View::Xf::at(double dx, double dy, double& ox, double& oy) const {
  const double lx = dx * sx, ly = dy * sy, c = std::cos(rot), s = std::sin(rot);
  ox = x + lx * c - ly * s;
  oy = y + lx * s + ly * c;
}

const Sprite* View::need(const char* name) {
  const Sprite* s = pack_->find(name);
  if (!s && !missing_) missing_ = name;
  return s;
}

View::Layer View::make(lv_obj_t* parent, const Sprite* sprite, bool mask, bool additive) {
  Layer l;
  l.obj = lv_image_create(parent);
  lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_SCROLLABLE);
  if (mask) lv_obj_set_style_image_recolor_opa(l.obj, LV_OPA_COVER, 0);
  if (additive) lv_image_set_blend_mode(l.obj, LV_BLEND_MODE_ADDITIVE);
  if (sprite) {
    lv_image_set_src(l.obj, &sprite->dsc);
    lv_image_set_pivot(l.obj, iround(sprite->ox), iround(sprite->oy));
    l.sprite = sprite;
  }
  hide(l);
  return l;
}

void View::hide(Layer& l) {
  if (!l.hidden) {
    lv_obj_add_flag(l.obj, LV_OBJ_FLAG_HIDDEN);
    l.hidden = true;
    touched(l.box);
    l.box = {0, 0, -1, -1};
  }
}

void View::tint(Layer& l, uint32_t rgb) {
  if (l.color != rgb) {
    lv_obj_set_style_image_recolor(l.obj, hex(rgb), 0);
    l.color = rgb;
    if (!l.hidden) touched(l.box);
  }
}

// Screen bounds of a layer as LVGL draws it: the sprite rect scaled then rotated about its pivot, then offset by
// the object position. Padded for anti-aliased edges and the 0.1 degree/1/256 rounding inside LVGL.
static lv_area_t layer_bounds(const Sprite* s, int32_t x, int32_t y, int32_t rot, uint32_t isx, uint32_t isy) {
  const double w = s->dsc.header.w, h = s->dsc.header.h, pvx = iround(s->ox), pvy = iround(s->oy);
  const double sx = isx / 256.0, sy = isy / 256.0, a = rot * kPi / 1800, c = std::cos(a), sn = std::sin(a);
  double x1 = 1e9, y1 = 1e9, x2 = -1e9, y2 = -1e9;
  const double cx[4] = {0, w, 0, w}, cy[4] = {0, 0, h, h};
  for (int i = 0; i < 4; ++i) {
    const double lx = (cx[i] - pvx) * sx, ly = (cy[i] - pvy) * sy;
    const double px = pvx + lx * c - ly * sn, py = pvy + lx * sn + ly * c;
    x1 = std::min(x1, px); x2 = std::max(x2, px); y1 = std::min(y1, py); y2 = std::max(y2, py);
  }
  return {x + static_cast<int32_t>(std::floor(x1)) - 3, y + static_cast<int32_t>(std::floor(y1)) - 3,
          x + static_cast<int32_t>(std::ceil(x2)) + 3, y + static_cast<int32_t>(std::ceil(y2)) + 3};
}

// Draws `sprite` so that its reference origin lands at (x, y), rotated by `rot` radians and
// scaled about that origin (Canvas translate -> rotate -> scale == LVGL scale then rotate).
void View::place(Layer& l, const Sprite* sprite, double x, double y, double rot, double sx, double sy, double opa) {
  if (!sprite || opa <= 0.002) { hide(l); return; }
  bool changed = l.hidden, moved = l.hidden;
  if (l.sprite != sprite) {
    lv_image_set_src(l.obj, &sprite->dsc);
    lv_image_set_pivot(l.obj, iround(sprite->ox), iround(sprite->oy));
    l.sprite = sprite;
    changed = moved = true;
  }
  const int32_t px = iround(x - sprite->ox), py = iround(y - sprite->oy);
  if (px != l.x || py != l.y) { lv_obj_set_pos(l.obj, px, py); l.x = px; l.y = py; moved = true; }
  int32_t r = iround(rot * 1800 / kPi) % 3600;
  if (r < 0) r += 3600;
  if (r != l.rot) { lv_image_set_rotation(l.obj, r); l.rot = r; moved = true; }
  const uint32_t isx = static_cast<uint32_t>(std::max<int32_t>(1, iround(sx * 256)));
  const uint32_t isy = static_cast<uint32_t>(std::max<int32_t>(1, iround(sy * 256)));
  if (isx != l.sx) { lv_image_set_scale_x(l.obj, isx); l.sx = isx; moved = true; }
  if (isy != l.sy) { lv_image_set_scale_y(l.obj, isy); l.sy = isy; moved = true; }
  const lv_opa_t o = to_opa(opa);
  if (o != l.opa) { lv_obj_set_style_image_opa(l.obj, o, 0); l.opa = o; changed = true; }
  if (l.hidden) { lv_obj_remove_flag(l.obj, LV_OBJ_FLAG_HIDDEN); l.hidden = false; }
  if (moved) {
    touched(l.box);
    l.box = layer_bounds(sprite, px, py, l.rot, l.sx, l.sy);
    touched(l.box);
  } else if (changed) {
    touched(l.box);
  }
}

// Places a sprite drawn inside the frame `f` whose own origin sits at local (dx, dy), with an
// extra uniform scale `k` about that origin.
void View::place_xf(Layer& l, const Sprite* sprite, const Xf& f, double dx, double dy, double k, double opa) {
  double x, y;
  f.at(dx, dy, x, y);
  place(l, sprite, x, y, f.rot, f.sx * k, f.sy * k, opa);
}

lv_obj_t* View::make_draw(lv_obj_t* screen, Pass pass) {
  const int i = static_cast<int>(pass);
  ctx_[i] = {this, pass};
  lv_obj_t* o = lv_obj_create(screen);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(o, kWidth, kHeight);
  lv_obj_add_event_cb(o, draw_event, LV_EVENT_DRAW_MAIN, &ctx_[i]);
  draw_[i] = o;
  return o;
}

bool View::create(lv_obj_t* screen, const AssetPack& pack, const char** missing) {
  pack_ = &pack;
  missing_ = nullptr;
  char n[32];
  auto get = [&](const char* fmt, auto... a) {
    std::snprintf(n, sizeof n, fmt, a...);
    return need(n);
  };
  speck_[0] = need("speck_1");
  speck_[1] = need("speck_2");
  for (int k = 0; k < 8; ++k) { glint_[k] = get("glint_%d", k); drip_swell_[k] = get("drip_swell_%d", k); }
  drip_fall_ = need("drip_fall");
  for (int k = 0; k < 12; ++k) drip_ring_[k] = get("drip_ring_%d", k);
  drip_drop_ = need("drip_drop");
  const Sprite* bin_body = need("bin_body");
  const Sprite* bin_lid = need("bin_lid");
  const Sprite* rec_body = need("rec_body");
  rec_dot_lit_ = need("rec_dot_lit");
  rec_dot_dim_ = need("rec_dot_dim");
  for (int r = 34; r <= 52; ++r) rec_arc_[r - 34] = get("rec_arc_%d", r);
  lid_s_ = need("lid");
  lid_glow_ = need("lid_glow");
  const Sprite* jar_back = need("jar_back");
  const Sprite* jar_front = need("jar_front");
  const Sprite* visor_body = need("visor_body");
  const Sprite* visor_bar = need("visor_bar");
  for (int li = 1; li <= 33; ++li) {
    for (int h = 0; h < 5; ++h) mist_body_[h][li - 1] = get("mist_%d_%d", h, li);
    for (int red = 0; red < 2; ++red)
      for (int k = 0; k < 8; ++k) {
        cap_f_[red][li - 1][k] = get("cap_%d_%d_%d", red, li, k);
        sheen_f_[red][li - 1][k] = get("sheen_%d_%d_%d", red, li, k);
      }
  }
  for (int li = 1; li <= 17; ++li)
    for (int k = 0; k < 16; ++k) swirl_f_[li - 1][k] = get("swirl_%d_%d", li, k);
  for (int k = 0; k < 8; ++k) {
    mote_bubble_[k] = get("mote_bubble_%d", k);
    mote_dot_[k] = get("mote_dot_%d", k);
    mote_heart_[k] = get("mote_heart_%d", k);
    for (int a = 0; a < 6; ++a) mote_star_[k][a] = get("mote_star_%d_%d", k, a);
  }
  const Sprite* cheeks = need("face_cheeks");
  eyes_open_s_ = need("eyes_open_s");
  eyes_open_l_ = need("eyes_open_l");
  pupils_s_ = need("pupils_s");
  pupils_l_ = need("pupils_l");
  eyes_shut_ = need("eyes_shut");
  eyes_happy_ = need("eyes_happy");
  const Sprite* brows = need("brows");
  mouth_hmm_ = need("mouth_hmm");
  mouth_o_ = need("mouth_o");
  mouth_happy_ = need("mouth_happy");
  mouth_sleep_ = need("mouth_sleep");
  mouth_smile_ = need("mouth_smile");
  for (int k = 0; k < 16; ++k) mouth_sing_[k] = get("mouth_sing_%d", k);
  for (int i = 0; i < 5; ++i) {
    pad_body_[i] = get("pad%d_body", i);
    pad_eyes_[i] = get("pad%d_eyes", i);
    pad_smile_[i] = get("pad%d_smile", i);
    pad_shut_[i] = get("pad%d_shut", i);
    pad_cheer_[i] = get("pad%d_cheer", i);
    pad_glow_[i] = get("pad%d_glow", i);
    for (int k = 0; k < 8; ++k) pad_sing_[i][k] = get("pad%d_sing_%d", i, k);
  }
  fxbody_off_ = need("fxbody_off");
  for (int i = 0; i < 4; ++i) { fx_body_[i] = get("fx%d_body", i); fx_icon_[i] = get("fx%d_icon", i); }
  for (int k = 0; k <= 8; ++k) { fx0_on_[k] = get("fx0_on_%d", k); fx2_on_[k] = get("fx2_on_%d", k); }
  fx1_dot_ = need("fx1_dot");
  fx1_ring_[0] = need("fx1_ring0");
  fx1_ring_[1] = need("fx1_ring1");
  for (int k = 0; k < 16; ++k) fx3_on_[k] = get("fx3_on_%d", k);
  for (int k = 0; k < 13; ++k) pcircle_[k] = get("pcircle_%d", k);
  for (int s = 0; s < 7; ++s)
    for (int a = 0; a < 6; ++a) pstar_[s][a] = get("pstar_%d_%d", s, a);
  for (int s = 0; s < 4; ++s)
    for (int a = 0; a < 12; ++a) prect_[s][a] = get("prect_%d_%d", s, a);
  for (int s = 0; s < 5; ++s) pnote_[s] = get("pnote_%d", s);
  const Sprite* bg = need("bg");
  const Sprite* hand = need("hand");
  const Sprite* home = need("chrome_home");
  const Sprite* mic = need("chrome_mic");
  const Sprite* veil = need("start_veil");
  const Sprite* start_button = need("start_button");
  if (missing_) { if (missing) *missing = missing_; return false; }

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

  // drawScene() order: background, ambient, bin, record, jar (+ lid), ribbon, pads, hint,
  // fx buttons, fx, chrome, helper hand; then drawStart().
  bg_ = make(screen, bg, false);
  place(bg_, bg, 0, 0);
  for (Layer& s : specks_) { s = make(screen, speck_[0], true); tint(s, kTeal.c[0]); }
  glint_l_ = make(screen, glint_[0], true, true);
  tint(glint_l_, kPurple.c[0]);
  drip_main_ = make(screen, drip_fall_, true);
  tint(drip_main_, kTeal.c[0]);
  for (Layer& d : drip_drops_) { d = make(screen, drip_drop_, true); tint(d, kTeal.c[0]); }
  bin_body_ = make(screen, bin_body, false);
  bin_lid_ = make(screen, bin_lid, false);
  for (Layer& d : rec_dots_) d = make(screen, rec_dot_dim_, false);
  for (Layer& a : rec_arcs_) a = make(screen, rec_arc_[0], true);
  rec_body_ = make(screen, rec_body, false);
  jar_back_ = make(screen, jar_back, false);
  mist_ = make(screen, mist_body_[0][0], false);
  cap_ = make(screen, cap_f_[0][0][0], true);
  swirl_ = make(screen, swirl_f_[0][0], true);
  for (Layer& m : motes_) { m = make(screen, mote_dot_[0], true); tint(m, kWhite); }
  sheen_ = make(screen, sheen_f_[0][0][0], true);
  tint(sheen_, kWhite);
  cheeks_ = make(screen, cheeks, false);
  eyes_ = make(screen, eyes_open_s_, false);
  pupils_ = make(screen, pupils_s_, false);
  brows_ = make(screen, brows, false);
  mouth_ = make(screen, mouth_smile_, false);
  visor_ = make(screen, visor_body, false);
  visor_bar_ = make(screen, visor_bar, true);
  tint(visor_bar_, kTeal.c[0]);
  jar_front_ = make(screen, jar_front, false);
  lid_glow_l_ = make(screen, lid_glow_, true, true);
  tint(lid_glow_l_, kTeal.c[0]);
  lid_ = make(screen, lid_s_, false);
  make_draw(screen, Pass::Ribbon);
  for (int i = 0; i < 5; ++i) {
    PadLayers& p = pads_[i];
    p.glow = make(screen, pad_glow_[i], true, true);
    tint(p.glow, kPads[i].hue.c[0]);
    p.body = make(screen, pad_body_[i], false);
    p.eyes = make(screen, pad_eyes_[i], false);
    p.mouth = make(screen, pad_smile_[i], false);
  }
  make_draw(screen, Pass::Overlay);
  for (int i = 0; i < 4; ++i) {
    FxLayers& f = fx_[i];
    f.body = make(screen, fxbody_off_, false);
    f.icon = make(screen, fx_icon_[i], true);
    f.ring0 = make(screen, fx1_ring_[0], true);
    f.ring1 = make(screen, fx1_ring_[1], true);
    tint(f.ring0, kWhite);
    tint(f.ring1, kWhite);
  }
  make_draw(screen, Pass::Fx);
  home_ = make(screen, home, false);
  mic_ = make(screen, mic, false);
  place(home_, home, 4, 4);
  place(mic_, mic, 268, 4);
  hand_ = make(screen, hand, false);

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
  // Every setter below would invalidate on its own (rotation/scale twice), overflowing LVGL's 32-entry
  // invalid-area buffer into full-screen redraws. Collect changed sprite bounds into tiles instead and
  // invalidate the merged rectangles once at the end.
  lv_display_t* display = lv_obj_get_display(veil_);
  lv_display_enable_invalidation(display, false);
  std::memset(sprites_, 0, sizeof(Tiles));
  box_count_ = 0;
  box_overflow_ = false;
  const double time = g.time();
  const bool reduced = g.reduced();
  const double m = reduced ? 0.3 : 1;
  const Phase phase = g.phase();

  // ---- drawAmbient(): specks, crystal glint, drips ----
  for (int i = 0; i < 16; ++i) {
    const int grp = i % 4;
    const double a = 0.32 + 0.22 * std::sin(time * (0.7 + grp * 0.17) + grp * 1.9) * m;
    place(specks_[i], speck_[i % 2], (i * 73 + 19) % kWidth, (i * 41 + 24) % 190 + 20, 0, 1, 1, a);
  }
  {
    const double gk = wrap(time, 4) / 4, ga = std::pow(std::sin(gk * kPi), 4);
    static constexpr int kGi[3] = {1, 3, 6};
    const int gi = kGi[static_cast<int>(std::floor(time / 4)) % 3];
    if (ga > 0.02) place(glint_l_, glint_[cyc(time * 0.5, kTau / 5, 8)], gi * 47 - 3, 50, 0, 1, 1, 0.7 * ga);
    else hide(glint_l_);
  }
  {
    static const double kTFall = std::sqrt(2 * (kLedge - 56) / kAmbG);
    bool shown = false;
    const double amb = g.amb();
    const int L = static_cast<int>(std::floor(amb / kAmbLoop));
    for (int l = L; l <= L + 1 && !shown; ++l) {
      for (const Drip& d : kDrips) {
        const double u = amb - (l * kAmbLoop + d.t);
        if (u < -1.1 || u >= 0.45) continue;
        const Lane& ln = kLanes[d.lane];
        const double dx = ln.x - kLanes[0].x;
        if (u < -kTFall) {
          const double k = (u + 1.1) / (1.1 - kTFall);
          const Sprite* s = drip_swell_[clampi(static_cast<int>(std::floor(k * 8)), 0, 7)];
          place(drip_main_, s, s->ox + dx, s->oy, 0, 1, 1, 0.85);
          hide(drip_drops_[0]); hide(drip_drops_[1]);
        } else if (u < 0) {
          place(drip_main_, drip_fall_, ln.x, ln.land - 0.5 * kAmbG * u * u, 0, 1, 1, 0.85);
          hide(drip_drops_[0]); hide(drip_drops_[1]);
        } else {
          const double k = u / 0.45, a = 0.6 * (1 - k), hy = std::sin(k * kPi) * 9 * m;
          const Sprite* s = drip_ring_[clampi(static_cast<int>(std::floor(k * 12)), 0, 11)];
          place(drip_main_, s, s->ox + dx, s->oy, 0, 1, 1, a);
          place(drip_drops_[0], drip_drop_, ln.x - 7 * k, ln.land - hy, 0, 1, 1, a);
          place(drip_drops_[1], drip_drop_, ln.x + 7 * k, ln.land - hy, 0, 1, 1, a);
        }
        shown = true;
        break;
      }
    }
    if (!shown) { hide(drip_main_); hide(drip_drops_[0]); hide(drip_drops_[1]); }
  }

  // ---- drawBin() ----
  {
    const Bin& b = g.bin();
    const double br = 1 + std::sin(time * kTau / 2.9) * 0.012 * m;
    const double sx = (1 - b.q) * br, sy = (1 + b.q) / br;
    place(bin_body_, bin_body_.sprite, kBinX, kLedge, 0, sx, sy);
    const double la = -b.lid * 1.1 * (reduced ? 0.6 : 1) - std::max(0.0, std::sin(time * 1.3)) * 0.04;
    place(bin_lid_, bin_lid_.sprite, kBinX + 21 * sx, kLedge - 47 * sy, la, sx, sy);
  }

  // ---- drawRecord() ----
  {
    const bool recording = phase == Phase::Recording;
    const RecButton& rb = g.rec();
    for (int i = 0; i < 3; ++i) {
      if (phase == Phase::Countdown || recording) {
        const double lit = recording ? 1 : clampd((g.phase_time() - i * 0.45) / 0.2, 0, 1);
        const double s = lit > 0 ? ease_out_back(lit) : 1;
        place(rec_dots_[i], lit > 0 ? rec_dot_lit_ : rec_dot_dim_, kRecX - 14 + 14 * i, 86, 0, s, s);
      } else {
        hide(rec_dots_[i]);
      }
    }
    if (recording || rb.louder > 0) {
      const double lv = recording ? clampd(g.input_level() / 0.12, 0, 1) : 0, k = 1 - rb.louder / 1.4;
      for (int j = 0; j < 3; ++j) {
        const double a = recording ? clampd(lv * 3 - j, 0, 1) : clampd(k * 4 - j, 0, 1) * (1 - k);
        if (a <= 0.02) { hide(rec_arcs_[j]); continue; }
        const double r = kRecR + 6 + 7 * j + (recording ? 0 : 3 * k);
        tint(rec_arcs_[j], recording ? kRed.c[0] : kHintGlow);
        place(rec_arcs_[j], rec_arc_[clampi(iround(r), 34, 52) - 34], kRecX, kRecY, 0, 1, 1, a);
      }
    } else {
      for (Layer& a : rec_arcs_) hide(a);
    }
    const double pulse = recording ? 1 + 0.04 * std::sin(time * kTau * 1.2) * m : 1 + std::sin(time * kTau / 2.4) * 0.015 * m;
    place(rec_body_, rec_body_.sprite, kRecX, kLedge - 1, wiggle(rb, reduced), (1 - rb.q) * pulse, (1 + rb.q) * pulse);
  }

  // ---- drawJar() ----
  const Jar& jar = g.jar();
  const Lid& lid = g.lid();
  {
    const double br = std::sin(time * kTau / 2.8) * 0.025 * m;
    const Xf f{kFootX, kFootY, jar.ang, (1 - jar.q) * (1 - br * 0.5), (1 + jar.q) * (1 + br)};
    place_xf(jar_back_, jar_back_.sprite, f, 0, 0);
    const Hue& hue = kTakes[jar.mist].hue;
    if (jar.level > 0.01) {
      double ys, w;
      mist_geom(jar.level, ys, w);
      const int li = clampi(iround(jar.level / 0.02), 1, 33);
      const int red = reduced ? 1 : 0, k = cyc(time, kTau / 2.4, 8);
      place_xf(mist_, mist_body_[jar.mist][li - 1], f, 0, 0);
      tint(cap_, hue.c[0]);
      place_xf(cap_, cap_f_[red][li - 1][k], f, 0, 0);
      tint(swirl_, hue.c[0]);
      place_xf(swirl_, swirl_f_[clampi(iround(jar.level / 0.04), 1, 17) - 1][cyc(jar.swirl, kPi, 16)], f, 0, 0);
      if (jar.level > 0.12) {
        const double dir = g.fx_on(kBack) ? -1 : 1;
        const Mote mote = kTakes[jar.mist].mote;
        for (int j = 0; j < 5; ++j) {
          const double ph = wrap(time * 0.22 * dir + j / 5.0, 1.0);
          const double y = lerp(30, ys + 4, ph), x = std::sin(j * 2.1 + time * 0.5) * w * 0.55;
          const double s = std::sin(ph * kPi);
          if (s < 0.15) { hide(motes_[j]); continue; }
          const int sk = clampi(iround((s - 0.15) / (0.85 / 7)), 0, 7);
          const Sprite* sp = mote == Mote::Heart  ? mote_heart_[sk]
                             : mote == Mote::Star  ? mote_star_[sk][cyc(time + j, kTau / 5, 6)]
                             : mote == Mote::Bubble ? mote_bubble_[sk]
                                                    : mote_dot_[sk];
          place_xf(motes_[j], sp, f, x, y + kBodyDy);
        }
      } else {
        for (Layer& l : motes_) hide(l);
      }
      place_xf(sheen_, sheen_f_[red][li - 1][k], f, 0, 0);
    } else {
      hide(mist_); hide(cap_); hide(swirl_); hide(sheen_);
      for (Layer& l : motes_) hide(l);
    }

    // jarFace(jarMood(), jar.mouth) at translate(0, 2): exported in place relative to the foot.
    enum { kSleep, kIdle, kListen, kHmm, kHappy } mood;
    if (phase == Phase::Start || (phase == Phase::Intro && g.phase_time() < 0.3)) mood = kSleep;
    else if (phase == Phase::Countdown || phase == Phase::Recording) mood = kListen;
    else mood = jar.hmm > 0 ? kHmm : jar.happy > 0 ? kHappy : kIdle;
    place_xf(cheeks_, cheeks_.sprite, f, 0, 0);
    if (mood == kSleep || mood == kHappy || jar.blink < 0) {
      place_xf(eyes_, mood == kHappy ? eyes_happy_ : eyes_shut_, f, 0, 0);
      hide(pupils_);
    } else if (mood == kListen) {
      place_xf(eyes_, eyes_open_l_, f, 0, 0);
      place_xf(pupils_, pupils_l_, f, 0, 0);
    } else {
      place_xf(eyes_, eyes_open_s_, f, 0, 0);
      const double lx0 = mood == kHmm ? 40 : jar.look_x, ly0 = mood == kHmm ? -60 : jar.look_y;
      const double ln = std::hypot(lx0, ly0) > 0 ? std::hypot(lx0, ly0) : 1;
      place_xf(pupils_, pupils_s_, f, lx0 / ln * 6.5 * 0.35, ly0 / ln * 6.5 * 0.35);
    }
    if (mood == kHmm) place_xf(brows_, brows_.sprite, f, 0, 0);
    else hide(brows_);
    if (mood == kListen || jar.hum > 0) {
      const double o = 1 + (jar.hum > 0 ? 0.25 * std::sin(jar.hum * kPi * 4.4) : 0);
      place_xf(mouth_, mouth_o_, f, 0, 131 - kFootY, o);  // mouth_o origin = the "o" centre (160, 131)
    } else if (mood == kHmm) {
      place_xf(mouth_, mouth_hmm_, f, 0, 0);
    } else if (jar.mouth > 0.05 && mood != kSleep) {
      place_xf(mouth_, mouth_sing_[clampi(iround(jar.mouth * 16) - 1, 0, 15)], f, 0, 0);
    } else if (mood == kHappy) {
      place_xf(mouth_, mouth_happy_, f, 0, 0);
    } else if (mood == kSleep) {
      place_xf(mouth_, mouth_sleep_, f, 0, 0);
    } else {
      place_xf(mouth_, mouth_smile_, f, 0, 0);
    }
    if (jar.visor > 0.01) {
      const double vy = -26 * (1 - jar.visor), va = std::min(1.0, jar.visor * 1.5);
      place_xf(visor_, visor_.sprite, f, 0, vy, 1, va);
      place_xf(visor_bar_, visor_bar_.sprite, f, std::sin(time * 2.5) * 18 * m, vy, 1, va * 0.6);
    } else {
      hide(visor_); hide(visor_bar_);
    }
    place_xf(jar_front_, jar_front_.sprite, f, 0, 0);

    // drawLid(): local (on the rim) while u == 0, else the world-space hop to the left shoulder.
    if (lid.u == 0) {
      const double lid_dy = kLidY - kFootY;
      if (phase == Phase::Empty) place_xf(lid_glow_l_, lid_glow_, f, 0, lid_dy, 1, 0.22 + 0.12 * std::sin(time * 2.2));
      else hide(lid_glow_l_);
      place_xf(lid_, lid_s_, f, 0, lid_dy);
    } else {
      hide(lid_glow_l_);
      const double e = ease_in_out(lid.u);
      place(lid_, lid_s_, lerp(kLidX, kLidRestX, e), lerp(kLidY, kLidRestY, e) - std::sin(lid.u * kPi) * 30,
            lerp(0, kLidRestA, e));
    }
  }

  // ---- drawPads() ----
  for (int i = 0; i < 5; ++i) {
    const Pad& p = g.pad(i);
    PadLayers& L = pads_[i];
    const double x = pad_x(i);
    const double hop_y = p.hop >= 0 && p.hop < 0.45 ? std::sin(p.hop / 0.45 * kPi) * 7 * m : 0;
    const double br = std::sin(time * kTau / (2.6 + 0.12 * i) + i) * 0.02 * m;
    const double sx = (1 - p.q) * (1 - 0.025 * p.bend) * (1 - br * 0.5), sy = (1 + p.q) * (1 + 0.05 * p.bend) * (1 + br);
    const double rot = wiggle(p, reduced), y = kPadFoot - hop_y;
    if (p.glow > 0.02) place(L.glow, pad_glow_[i], x, y, rot, sx, sy, 0.35 * p.glow);
    else hide(L.glow);
    place(L.body, pad_body_[i], x, y, rot, sx, sy);
    const bool asleep = phase == Phase::Start || !p.awake, cheer = p.hop >= 0 && p.hop < 0.6;
    place(L.eyes, cheer ? pad_cheer_[i] : (asleep || p.blink < 0) ? pad_shut_[i] : pad_eyes_[i], x, y, rot, sx, sy);
    const Sprite* mouth =
        p.sing > 0.03 ? pad_sing_[i][clampi(iround(clampd(p.sing / 0.15, 0, 1) * 8) - 1, 0, 7)] : pad_smile_[i];
    place(L.mouth, mouth, x, y, rot, sx, sy);
  }

  // ---- drawFxButtons() ----
  for (int i = 0; i < 4; ++i) {
    const FxButton& b = g.fx_button(i);
    FxLayers& L = fx_[i];
    const bool on = b.on > 0.5;
    const double rs = 1 + 0.08 * b.on, y = kFxY + std::sin(time * 2.1 + i * 1.7) * 2 * m;
    const double x = kFx[i].x, rot = wiggle(b, reduced), sx = 1 - b.q, sy = 1 + b.q;
    if (on) place(L.body, fx_body_[i], x, y, rot, sx * rs / 1.08, sy * rs / 1.08);
    else place(L.body, fxbody_off_, x, y, rot, sx * rs, sy * rs);
    hide(L.ring0);
    hide(L.ring1);
    if (!on) {
      tint(L.icon, kInkSoft);
      place(L.icon, fx_icon_[i], x, y, rot, sx, sy);
      continue;
    }
    tint(L.icon, kWhite);
    switch (i) {
      case kBack:
        place(L.icon, fx0_on_[clampi(iround(std::fabs(std::sin(time * kPi * 1.2)) * m * 8), 0, 8)], x, y, rot, sx, sy);
        break;
      case kEcho:
        place(L.icon, fx1_dot_, x, y, rot, sx, sy);
        place(L.ring0, fx1_ring_[0], x, y, rot, sx, sy, 0.6 + 0.4 * std::sin(time * 5 * m));
        place(L.ring1, fx1_ring_[1], x, y, rot, sx, sy, 0.6 + 0.4 * std::sin(time * 5 * m - 1.4));
        break;
      case kRobot: {
        const double by = std::sin(time * 6) * 1.2 * m;
        place(L.icon, fx2_on_[clampi(iround((by + 1.2) / 0.3), 0, 8)], x, y, rot, sx, sy);
        break;
      }
      default:
        place(L.icon, fx3_on_[cyc(time * 5 * m, kTau, 16)], x, y, rot, sx, sy);
        break;
    }
  }

  // ---- custom-draw passes (ribbon, overlay, fx): what they touched is invalidated at the end ----
  Tiles now{};
  dirty_tiles(g, now);

  // ---- drawHelperHand() ----
  const Suggestion s = g.hint() >= 2 ? g.suggest() : Suggestion{Target::None, 0, 0, 0, 0};
  if (s.kind != Target::None)
    place(hand_, hand_.sprite, s.x + 8, s.y + 16 - std::fabs(std::sin(time * 4)) * 6 * m);
  else
    hide(hand_);

  // ---- drawStart() ----
  if (phase == Phase::Start) {
    if (lv_obj_has_flag(veil_, LV_OBJ_FLAG_HIDDEN)) touched({0, 0, kWidth - 1, kHeight - 1});
    lv_obj_remove_flag(veil_, LV_OBJ_FLAG_HIDDEN);
    const double p = 1 + std::sin(time * 3) * 0.04;
    place(start_button_, start_button_.sprite, 160, 120, 0, p, p);
  } else {
    if (!lv_obj_has_flag(veil_, LV_OBJ_FLAG_HIDDEN)) touched({0, 0, kWidth - 1, kHeight - 1});
    lv_obj_add_flag(veil_, LV_OBJ_FLAG_HIDDEN);
    hide(start_button_);
  }

  lv_obj_update_layout(veil_);  // apply moved positions while invalidation is still suspended
  lv_display_enable_invalidation(display, true);
  invalidate(now);
  std::memcpy(prev_, now, sizeof(Tiles));
}

void View::mark(Tiles& t, double x1, double y1, double x2, double y2) const {
  if (x2 < 0 || y2 < 0 || x1 >= kWidth || y1 >= kHeight) return;
  const int tx1 = clampi(static_cast<int>(std::floor(x1)) / kTile, 0, kTilesX - 1);
  const int tx2 = clampi(static_cast<int>(std::ceil(x2)) / kTile, 0, kTilesX - 1);
  const int ty1 = clampi(static_cast<int>(std::floor(y1)) / kTile, 0, kTilesY - 1);
  const int ty2 = clampi(static_cast<int>(std::ceil(y2)) / kTile, 0, kTilesY - 1);
  for (int ty = ty1; ty <= ty2; ++ty)
    for (int tx = tx1; tx <= tx2; ++tx) t[ty][tx] = 1;
}

void View::dirty_tiles(const Game& g, Tiles& tiles) {
  const double m = g.reduced() ? 0.3 : 1;
  if (g.phase() == Phase::Recording) mark(tiles, 150, 40, 262, 122);
  for (int i = 0; i < 5; ++i) {
    if (!g.pad_looping(i)) continue;
    const double r = kPadR * kPads[i].s, e = r + 11;
    mark(tiles, pad_x(i) - e, kPadFoot - r - e, pad_x(i) + e, kPadFoot - r + e);
  }
  if (g.hint() >= 1) {
    const Suggestion s = g.suggest();
    if (s.kind != Target::None) mark(tiles, s.x - s.r - 4, s.y - s.r - 4, s.x + s.r + 4, s.y + s.r + 4);
  }
  for (int i = 0; i < 4; ++i) {
    const FxButton& b = g.fx_button(i);
    if (b.on <= 0.05) continue;
    const double y = kFxY + std::sin(g.time() * 2.1 + i * 1.7) * 2 * m, e = kFxR * 1.08 + 4 + 1.5 + 4;
    mark(tiles, kFx[i].x - e, y - e, kFx[i].x + e, y + e);
  }
  for (const Ring& r : g.rings()) {
    if (r.t < 0) continue;
    const double e = std::max(r.r0, r.r1) + r.w + 2;
    mark(tiles, r.x - e, r.y - e, r.x + e, r.y + e);
  }
  for (const Particle& p : g.particles()) mark(tiles, p.x - 12, p.y - 14, p.x + 12, p.y + 12);
}

int View::tile_rects(const Tiles& now, lv_area_t* rects) const {
  // Horizontal runs per tile row (now | prev_), merged downwards while the run is identical; the
  // union box when the tiles fragment into more than kMaxRects rectangles.
  int n = 0;
  bool overflow = false;
  int32_t bx1 = kWidth, by1 = kHeight, bx2 = -1, by2 = -1;
  int open[kTilesX];
  for (int& o : open) o = -1;
  for (int ty = 0; ty < kTilesY; ++ty) {
    int next_open[kTilesX];
    for (int& o : next_open) o = -1;
    for (int tx = 0; tx < kTilesX;) {
      if (!(now[ty][tx] | prev_[ty][tx])) { ++tx; continue; }
      int end = tx;
      while (end + 1 < kTilesX && (now[ty][end + 1] | prev_[ty][end + 1])) ++end;
      const int32_t x1 = tx * kTile, x2 = (end + 1) * kTile - 1, y1 = ty * kTile, y2 = (ty + 1) * kTile - 1;
      bx1 = std::min(bx1, x1); by1 = std::min(by1, y1); bx2 = std::max(bx2, x2); by2 = std::max(by2, y2);
      if (!overflow) {
        const int prev = open[tx];
        if (prev >= 0 && rects[prev].x2 == x2) {
          rects[prev].y2 = y2;
          next_open[tx] = prev;
        } else if (n < kMaxRects) {
          rects[n] = {x1, y1, x2, y2};
          next_open[tx] = n++;
        } else {
          overflow = true;
        }
      }
      tx = end + 1;
    }
    std::memcpy(open, next_open, sizeof(open));
  }
  if (bx2 < 0) return 0;
  if (overflow) { rects[0] = {bx1, by1, bx2, by2}; return 1; }
  return n;
}

void View::touched(const lv_area_t& b) {
  if (b.x2 < b.x1) return;
  mark(sprites_, b.x1, b.y1, b.x2, b.y2);
  const lv_area_t c{std::max<int32_t>(b.x1, 0), std::max<int32_t>(b.y1, 0), std::min<int32_t>(b.x2, kWidth - 1),
                    std::min<int32_t>(b.y2, kHeight - 1)};
  if (c.x2 < c.x1 || c.y2 < c.y1) return;
  if (box_count_ < kMaxBoxes) boxes_[box_count_++] = c;
  else box_overflow_ = true;
}

void View::invalidate(const Tiles& now) {
  // LVGL keeps at most LV_INV_BUF_SIZE (32) invalid areas per frame and then redraws the whole screen.
  // Prefer the exact sprite bounds (joined where the union is no larger than the parts, as LVGL does);
  // when they still do not fit, fall back to the coarser 20 px tiles.
  static auto area = [](const lv_area_t& a) { return int64_t(a.x2 - a.x1 + 1) * (a.y2 - a.y1 + 1); };
  for (bool joined = true; joined;) {
    joined = false;
    for (int i = 0; i < box_count_; ++i)
      for (int j = i + 1; j < box_count_; ++j) {
        const lv_area_t& a = boxes_[i];
        const lv_area_t& b = boxes_[j];
        if (a.x1 > b.x2 + 1 || b.x1 > a.x2 + 1 || a.y1 > b.y2 + 1 || b.y1 > a.y2 + 1) continue;
        const lv_area_t u{std::min(a.x1, b.x1), std::min(a.y1, b.y1), std::max(a.x2, b.x2), std::max(a.y2, b.y2)};
        if (area(u) > area(a) + area(b)) continue;
        boxes_[i] = u;
        boxes_[j--] = boxes_[--box_count_];
        joined = true;
      }
  }
  lv_area_t rects[kMaxRects];
  int n = tile_rects(now, rects);
  lv_obj_t* target = draw_[static_cast<int>(Pass::Fx)];
  if (!box_overflow_ && n + box_count_ <= kMaxRects) {
    for (int i = 0; i < box_count_; ++i) rects[n++] = boxes_[i];
  } else {
    Tiles all;
    for (int ty = 0; ty < kTilesY; ++ty)
      for (int tx = 0; tx < kTilesX; ++tx) all[ty][tx] = now[ty][tx] | sprites_[ty][tx];
    n = tile_rects(all, rects);
  }
  for (int i = 0; i < n; ++i) lv_obj_invalidate_area(target, &rects[i]);
}

void View::draw_event(lv_event_t* e) {
  auto* c = static_cast<DrawCtx*>(lv_event_get_user_data(e));
  if (c->view->game_) c->view->draw(lv_event_get_layer(e), c->pass);
}

void View::draw(lv_layer_t* layer, Pass pass) {
  const Game& g = *game_;
  const double time = g.time(), m = g.reduced() ? 0.3 : 1;
  // LVGL calls this once per invalidated area and draw-buffer stripe; only items touching the
  // current clip area may create draw tasks (each one is a heap allocation plus dispatch).
  const lv_area_t clip = layer->_clip_area;
  auto visible = [&](const lv_area_t& a) {
    return a.x2 >= clip.x1 && a.x1 <= clip.x2 && a.y2 >= clip.y1 && a.y1 <= clip.y2;
  };
  auto blit = [&](const Sprite* s, double x, double y, uint32_t rgb, double opa, bool additive) {
    if (!s || opa <= 0.002) return;
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
  auto circle = [&](double x, double y, double r, double w, uint32_t rgb, double opa) {
    if (opa <= 0.002) return;
    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = hex(rgb);
    a.opa = to_opa(opa);
    a.width = std::max<int32_t>(1, iround(w));
    a.center.x = iround(x);
    a.center.y = iround(y);
    a.radius = static_cast<uint16_t>(std::max<int32_t>(1, iround(r + w / 2)));
    a.start_angle = 0;
    a.end_angle = 360;
    const lv_area_t box{a.center.x - a.radius, a.center.y - a.radius, a.center.x + a.radius, a.center.y + a.radius};
    if (visible(box)) lv_draw_arc(layer, &a);
  };
  auto disc = [&](double r) { return pcircle_[clampi(iround((r - 1) / 0.5), 0, 12)]; };

  if (pass == Pass::Ribbon) {
    // drawRibbon(): only while RECORDING.
    if (g.phase() != Phase::Recording || g.take() < 0) return;
    const Hue& hue = kTakes[g.take()].hue;
    const double lv = clampd(g.input_level() / 0.12, 0, 1);
    for (int i = 0; i <= 24; ++i) {
      const double u = i / 24.0;
      double px, py;
      ribbon_pt(u, px, py);
      const double dx = 2 * (1 - u) * (kCx - kP0x) + 2 * u * (kP1x - kCx), dy = 2 * (1 - u) * (kCy - kP0y) + 2 * u * (kP1y - kCy);
      const double ln = std::hypot(dx, dy) > 0 ? std::hypot(dx, dy) : 1;
      const double off = std::sin(u * 14 - time * 9 * m) * (1.5 + 7 * lv) * std::sin(u * kPi);
      ribbon_[i] = {static_cast<lv_value_precise_t>(px - dy / ln * off), static_cast<lv_value_precise_t>(py + dx / ln * off)};
    }
    const lv_area_t box{140, 30, 270, 130};
    if (!visible(box)) return;
    const struct { uint32_t col; double w, a; } strokes[2] = {{hue.c[1], 3 + 3 * lv, 0.85}, {hue.c[0], 1.4 + lv, 1}};
    for (const auto& st : strokes) {
      lv_draw_line_dsc_t d;
      lv_draw_line_dsc_init(&d);
      d.points = ribbon_;
      d.point_cnt = 25;
      d.color = hex(st.col);
      d.width = std::max<int32_t>(1, iround(st.w));
      d.opa = to_opa(st.a);
      d.round_start = 1;
      d.round_end = 1;
      lv_draw_line(layer, &d);
    }
    for (int k = 0; k < 5; ++k) {
      double px, py;
      ribbon_pt(wrap(time * 0.9 + k / 5.0, 1.0), px, py);
      blit(disc(1.2 + 1.6 * lv), px, py, kWhite, 0.8, false);
    }
    return;
  }

  if (pass == Pass::Overlay) {
    // drawPads() loop dots, drawHint() and the drawFxButtons() halos.
    for (int i = 0; i < 5; ++i) {
      if (!g.pad_looping(i)) continue;
      const double r = kPadR * kPads[i].s, a0 = time * kPi * m, cy = kPadFoot - r;
      for (int k = 0; k < 8; ++k) {
        const double a = a0 + k * kPi / 4;
        blit(pcircle_[2], pad_x(i) + std::cos(a) * (r + 8), cy + std::sin(a) * (r + 8), kPads[i].hue.c[0], 1, false);
      }
    }
    if (g.hint() >= 1) {
      const Suggestion s = g.suggest();
      if (s.kind != Target::None) circle(s.x, s.y, s.r, 4, kHintGlow, 0.4 + 0.4 * std::sin(time * kTau / 0.9));
    }
    for (int i = 0; i < 4; ++i) {
      const FxButton& b = g.fx_button(i);
      if (b.on <= 0.05) continue;
      const double r = kFxR * (1 + 0.08 * b.on), y = kFxY + std::sin(time * 2.1 + i * 1.7) * 2 * m;
      circle(kFx[i].x, y, r + 4 + 1.5 * std::sin(time * kTau * 0.8) * m, 2.5, kFx[i].hue.c[0], 0.5 * b.on);
    }
    return;
  }

  // drawFx(): rings (after their delay), then particles.
  for (const Ring& r : g.rings()) {
    if (r.t < 0) continue;
    const double k = r.t / r.dur;
    circle(r.x, r.y, lerp(r.r0, r.r1, ease_out_cubic(k)), r.w, r.color, r.a * (1 - k));
  }
  for (const Particle& p : g.particles()) {
    const double opa = clampd(p.life / p.max * 1.6, 0, 1);
    const Sprite* s = nullptr;
    switch (p.shape) {
      case Shape::Circle: s = disc(p.size * (0.5 + 0.5 * p.life / p.max)); break;
      case Shape::Star: s = pstar_[clampi(iround((p.size - 3.5) / 0.5), 0, 6)][cyc(p.rot, kTau / 5, 6)]; break;
      case Shape::Rect: s = prect_[clampi(iround(p.size - 4.5), 0, 3)][cyc(p.rot, kPi, 12)]; break;
      case Shape::Note: s = pnote_[clampi(iround((p.size - 3) / 0.5), 0, 4)]; break;
    }
    blit(s, p.x, p.y, p.color, opa, p.additive);
  }
}

}  // namespace sound_jar
