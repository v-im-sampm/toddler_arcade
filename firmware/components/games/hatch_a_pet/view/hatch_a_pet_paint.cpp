#include "little_arcade/games/hatch_a_pet_paint.hpp"

#include <cmath>
#include <initializer_list>

namespace little_arcade::hatch_a_pet {

namespace {
using ui::c2d::Cap;
using ui::c2d::Context;
using ui::c2d::Join;
using ui::c2d::Op;
using ui::c2d::rgb;

constexpr float kPi = 3.14159265358979323846f;
constexpr float W = 320, H = 240;
constexpr uint32_t kInk = 0x3B3550, kCream = 0xFFF3D6, kHintGlow = 0xFFE066;
constexpr uint32_t kSkyTop = 0xBFE6FF, kSkyBot = 0xFFF1D9, kHillFar = 0xA8DDA0, kHillNear = 0x7CCB7A,
                   kGrassBot = 0x5DB463;
constexpr uint32_t kShell = 0xFFF6E2, kShellShadow = 0xE3CBA0, kShellLine = 0xD9C7A3;
constexpr float kGlowPeriod = 0.9f, kIdleVisualHint = 6, kBob = 2;
constexpr float kEggX = HatchAPetGame::kEggX, kEggY = HatchAPetGame::kEggY, kEggRx = HatchAPetGame::kEggRx,
                 kEggRy = HatchAPetGame::kEggRy, kCrackY = HatchAPetGame::kCrackY;
constexpr float kNestX = HatchAPetGame::kNestX, kNestY = HatchAPetGame::kNestY, kNestRx = HatchAPetGame::kNestRx;
constexpr float kChoiceR = HatchAPetGame::kChoiceR;
constexpr float kFlatTol = 0.2f;  // device px

float clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
float ease_out_cubic(float t) {
  const float u = 1 - t;
  return 1 - u * u * u;
}
float ease_out_back(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1, u = t - 1;
  return 1 + c3 * u * u * u + c1 * u * u;
}
// Single-precision trig: the ESP32-S3 has no float FPU, so std::sin(float)
// is emulated in software and dominates frame time.
inline float fsin(float a) { return std::sin(static_cast<float>(a)); }
inline float fcos(float a) { return std::cos(static_cast<float>(a)); }

// ---------- drawing helpers ----------
void star_path(Context& ctx, float cx, float cy, float ro, float ri, float rot) {
  ctx.begin_path();
  for (int i = 0; i < 10; ++i) {
    const float r = i % 2 ? ri : ro, a = rot + i * kPi / 5;
    ctx.line_to(cx + fcos(a) * r, cy + fsin(a) * r);
  }
  ctx.close_path();
}

void heart_path(Context& ctx, float cx, float cy, float s) {
  ctx.begin_path();
  ctx.move_to(cx, cy + s * 0.55f);
  ctx.bezier_curve_to(cx - s * 1.1f, cy - s * 0.1f, cx - s * 0.5f, cy - s * 0.95f, cx, cy - s * 0.35f);
  ctx.bezier_curve_to(cx + s * 0.5f, cy - s * 0.95f, cx + s * 1.1f, cy - s * 0.1f, cx, cy + s * 0.55f);
}

void leaf_path(Context& ctx, float s) {
  ctx.begin_path();
  ctx.move_to(-s * 0.8f, s * 0.6f);
  ctx.quadratic_curve_to(-s * 0.8f, -s * 0.8f, s * 0.8f, -s * 0.6f);
  ctx.quadratic_curve_to(s * 0.8f, s * 0.8f, -s * 0.8f, s * 0.6f);
}

void round_rect(Context& ctx, float x, float y, float w, float h, float r) {
  ctx.begin_path();
  ctx.move_to(x + r, y);
  ctx.arc_to(x + w, y, x + w, y + h, r);
  ctx.arc_to(x + w, y + h, x, y + h, r);
  ctx.arc_to(x, y + h, x, y, r);
  ctx.arc_to(x, y, x + w, y, r);
  ctx.close_path();
}

// Egg outline: an ellipse narrowed towards the top. t = 0 is the top, t = PI
// the bottom.
inline void egg_pt(float t, float rx, float ry, float& x, float& y) {
  const float s = fsin(t), c = fcos(t);
  x = rx * s * (1 - 0.1f * c);
  y = -ry * c;
}

void egg_path(Context& ctx, float rx, float ry) {
  ctx.begin_path();
  for (int i = 0; i < 48; ++i) {
    float x, y;
    egg_pt(i / 48.0f * kPi * 2, rx, ry, x, y);
    if (i) ctx.line_to(x, y);
    else ctx.move_to(x, y);
  }
  ctx.close_path();
}

// TC / HWC: the crack angle and the egg half-width at the crack.
const float kTc = std::acos(-kCrackY / kEggRy);
const float kHwc = [] {
  float x, y;
  egg_pt(kTc, kEggRx, kEggRy, x, y);
  return x;
}();
const float kZig[10][2] = {{-kHwc, -14}, {-36, -20}, {-26, -9}, {-15, -21}, {-4, -8},
                            {7, -21},     {18, -9},   {29, -20}, {39, -10},  {kHwc, -14}};

// top: zig then over the top; bottom: zig then under.
void half_path(Context& ctx, bool top) {
  ctx.begin_path();
  for (int i = 0; i < 10; ++i) {
    if (i) ctx.line_to(kZig[i][0], kZig[i][1]);
    else ctx.move_to(kZig[i][0], kZig[i][1]);
  }
  for (int i = 1; i < 24; ++i) {
    const float t = top ? kTc - i / 24.0f * 2 * kTc : kTc + i / 24.0f * (2 * kPi - 2 * kTc);
    float x, y;
    egg_pt(t, kEggRx, kEggRy, x, y);
    ctx.line_to(x, y);
  }
  ctx.close_path();
}

// Egg decals in egg-local px; every decal sits fully above or below the crack
// band (y -22..-6).
constexpr float kSpots[][3] = {{-20, -40, 7}, {14, -48, 5}, {22, -30, 5}, {-4, -54, 4},  {-26, 8, 8}, {6, 4, 6},
                                {28, 14, 6},   {-12, 30, 7}, {16, 36, 8},  {-30, 40, 5}, {0, 52, 6}};
constexpr float kStripes[] = {-38, 6, 34};
constexpr float kZigzags[] = {-40, 22};
constexpr float kStars[][4] = {{-16, -40, 7, 0.2f}, {16, -46, 5, -0.3f}, {18, -30, 4, 0.5f}, {-22, 10, 8, 0.1f},
                                {16, 6, 6, -0.2f},   {0, 34, 8, 0.3f},    {-26, 40, 5, 0.4f}, {26, 32, 5, -0.1f}};

float hw_at(float y) {
  const float c = clamp(-y / kEggRy, -1, 1);
  float x, yy;
  egg_pt(std::acos(static_cast<float>(c)), kEggRx, kEggRy, x, yy);
  return x;
}

enum class Part : uint8_t { WHOLE, TOP, BOT };

void draw_decals(Context& ctx, EggPattern pattern, uint32_t color, Part part) {
  auto keep = [part](float y) { return part == Part::WHOLE || (part == Part::TOP ? y < kCrackY : y > kCrackY); };
  ctx.set_fill(rgb(color));
  ctx.set_stroke(rgb(color));
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_join(Join::ROUND);
  if (pattern == EggPattern::SPOTS) {
    for (const auto& s : kSpots) {
      if (!keep(s[1])) continue;
      ctx.begin_path();
      ctx.arc(s[0], s[1], s[2], 0, 7);
      ctx.fill();
    }
  } else if (pattern == EggPattern::STRIPES) {
    ctx.set_line_width(7);
    for (const float y0 : kStripes) {
      if (!keep(y0)) continue;
      const float w = hw_at(y0) - 7;
      ctx.begin_path();
      for (int i = 0; i <= 16; ++i) {
        const float x = -w + i / 16.0f * 2 * w;
        ctx.line_to(x, y0 + fsin(i / 16.0f * kPi * 3) * 2.5f);
      }
      ctx.stroke();
    }
  } else if (pattern == EggPattern::ZIGZAGS) {
    ctx.set_line_width(4);
    for (const float y0 : kZigzags) {
      if (!keep(y0)) continue;
      const float w = hw_at(y0) - 8;
      const long r = std::lround(w / 5);
      const int n = r > 2 ? static_cast<int>(r) : 2;
      ctx.begin_path();
      for (int i = 0; i <= n; ++i) ctx.line_to(-w + static_cast<float>(i) / n * 2 * w, y0 + (i % 2 ? -5 : 5));
      ctx.stroke();
    }
  } else {
    for (const auto& s : kStars) {
      if (!keep(s[1])) continue;
      star_path(ctx, s[0], s[1], s[2], s[2] * 0.45f, -kPi / 2 + s[3]);
      ctx.fill();
    }
  }
}

void shell_fill(Context& ctx, Part part) {
  const int g = ctx.create_radial_gradient(-16, -24, 6, 0, 0, 72);
  ctx.add_color_stop(g, 0, rgb(0xFFFFFF));
  ctx.add_color_stop(g, 0.5f, rgb(kShell));
  ctx.add_color_stop(g, 1, rgb(kShellShadow));
  ctx.set_fill_gradient(g);
  if (part == Part::WHOLE) egg_path(ctx, kEggRx, kEggRy);
  else half_path(ctx, part == Part::TOP);
  ctx.fill();
  ctx.set_stroke(rgb(kShellLine));
  ctx.set_line_width(2);
  ctx.set_line_join(Join::ROUND);
  ctx.stroke();
}

void shell_shine(Context& ctx) {
  ctx.set_fill(rgb(0xFFFFFF, 0.7f));
  ctx.begin_path();
  ctx.ellipse(-20, -32, 7, 13, 0.35f, 0, 7);
  ctx.fill();
  ctx.begin_path();
  ctx.arc(-13, -50, 2.2f, 0, 7);
  ctx.fill();
}

// The zig clipped to |x| <= lim.
int crack_line(float lim, float (*pts)[2]) {
  int n = 0;
  for (int i = 0; i < 10; ++i) {
    const float x = kZig[i][0], y = kZig[i][1];
    if (std::fabs(x) <= lim) {
      pts[n][0] = x;
      pts[n][1] = y;
      ++n;
    }
    if (i + 1 < 10) {
      const float x2 = kZig[i + 1][0], y2 = kZig[i + 1][1];
      for (const float edge : {-lim, lim}) {
        if ((x - edge) * (x2 - edge) < 0) {
          const float k = (edge - x) / (x2 - x);
          pts[n][0] = edge;
          pts[n][1] = y + (y2 - y) * k;
          ++n;
        }
      }
    }
  }
  return n;
}

void draw_crack(Context& ctx, const Egg& egg, float p, bool peek) {
  const float lim = p * kHwc * 1.05f;
  float pts[32][2];
  const int n = crack_line(lim, pts);
  if (n >= 2) {
    ctx.set_line_cap(Cap::ROUND);
    ctx.set_line_join(Join::ROUND);
    ctx.set_stroke(rgb(0xFFFFFF, 0.7f));
    ctx.set_line_width(1.2f);
    ctx.begin_path();
    for (int i = 0; i < n; ++i) {
      if (i) ctx.line_to(pts[i][0], pts[i][1] + 1.6f);
      else ctx.move_to(pts[i][0], pts[i][1] + 1.6f);
    }
    ctx.stroke();
    ctx.set_stroke(rgb(0x7A6A55));
    ctx.set_line_width(2.2f);
    ctx.begin_path();
    for (int i = 0; i < n; ++i) {
      if (i) ctx.line_to(pts[i][0], pts[i][1]);
      else ctx.move_to(pts[i][0], pts[i][1]);
    }
    ctx.stroke();
  }
  if (p >= 0.5f || peek) {  // a chip pops off
    ctx.set_fill(rgb(kInk));
    ctx.begin_path();
    ctx.move_to(-6, -9);
    ctx.line_to(6, -21);
    ctx.line_to(17, -10);
    ctx.line_to(6, -5);
    ctx.close_path();
    ctx.fill();
  }
  if (p >= 0.75f || peek) {  // somebody peeks out
    if (egg.blink < 0) {
      ctx.set_stroke(rgb(0xFFFFFF));
      ctx.set_line_width(1.5f);
      ctx.begin_path();
      ctx.move_to(3, -12);
      ctx.line_to(9, -12);
      ctx.stroke();
    } else {
      float ln = std::hypot(static_cast<float>(egg.look_x), static_cast<float>(egg.look_y));
      if (ln == 0) ln = 1;
      ctx.set_fill(rgb(0xFFFFFF));
      ctx.begin_path();
      ctx.arc(6, -12, 3.6f, 0, 7);
      ctx.fill();
      ctx.set_fill(rgb(kInk));
      ctx.begin_path();
      ctx.arc(6 + egg.look_x / ln * 1.2f, -12 + egg.look_y / ln * 1.2f, 1.7f, 0, 7);
      ctx.fill();
    }
  }
}

// ---------- background ----------
struct Flower {
  float x, y;
  uint32_t col;
};
struct Tuft {
  float x, y;
};
struct Straw {
  float u, ry, tilt;
  bool light;
};

const Flower* flowers() {
  static Flower out[9];
  static bool ready = false;
  if (!ready) {
    Mulberry32 r(7);
    constexpr uint32_t cols[3] = {0xFFFFFF, 0xFFB3DD, 0xFFEB9C};
    for (int i = 0; i < 9; ++i) {
      float x = 8 + static_cast<float>(r.next()) * 304;
      if (x > 92 && x < 228) x = x < 160 ? x - 84 : x + 84;
      const float y = 162 + static_cast<float>(r.next()) * 34;
      const uint32_t col = cols[static_cast<int>(static_cast<float>(r.next()) * 3)];
      out[i] = Flower{clamp(x, 8, 312), y, col};
    }
    ready = true;
  }
  return out;
}

const Tuft* tufts() {
  static Tuft out[22];
  static bool ready = false;
  if (!ready) {
    Mulberry32 r(9);
    for (int i = 0; i < 22; ++i) {
      const float x = static_cast<float>(r.next()) * W;
      out[i] = Tuft{x, 150 + static_cast<float>(r.next()) * 86};
    }
    ready = true;
  }
  return out;
}

const Straw* straws() {
  static Straw out[28];
  static bool ready = false;
  if (!ready) {
    Mulberry32 r(11);
    for (int i = 0; i < 28; ++i) {
      const float u = 0.08f + static_cast<float>(r.next()) * 0.84f;
      const float ry = 12 + static_cast<float>(r.next()) * 9;
      const float tilt = (static_cast<float>(r.next()) - 0.5f) * 0.5f;
      out[i] = Straw{u, ry, tilt, static_cast<float>(r.next()) < 0.5f};
    }
    ready = true;
  }
  return out;
}

// The moving parts of drawBackground(): clouds, swaying flowers, fluff.
void draw_background(Context& ctx, const HatchAPetGame& game) {
  const float time = static_cast<float>(game.time());
  ctx.set_fill(rgb(0xFFFFFF, 0.85f));  // drifting clouds
  constexpr float clouds[2][3] = {{40, 22, 4}, {210, 52, 2.6f}};
  for (const auto& c : clouds) {
    const float x = std::fmod(c[0] + time * c[2], W + 80) - 40, y = c[1];
    ctx.begin_path();
    ctx.arc(x, y, 9, 0, 7);
    ctx.arc(x + 11, y - 4, 11, 0, 7);
    ctx.arc(x + 23, y, 8, 0, 7);
    ctx.fill();
  }
  const Flower* fl = flowers();
  for (int f = 0; f < 9; ++f) {
    const float x = fl[f].x, y = fl[f].y;
    const float sway = fsin(time * 1.6f + x) * 1.2f;
    ctx.set_stroke(rgb(0x3A9442));
    ctx.set_line_width(1.5f);
    ctx.begin_path();
    ctx.move_to(x, y + 8);
    ctx.line_to(x + sway, y);
    ctx.stroke();
    ctx.set_fill(rgb(fl[f].col));
    for (int i = 0; i < 5; ++i) {
      const float a = i * kPi * 2 / 5;
      ctx.begin_path();
      ctx.arc(x + sway + fcos(a) * 3, y + fsin(a) * 3, 2.4f, 0, 7);
      ctx.fill();
    }
    ctx.set_fill(rgb(0xFFD23F));
    ctx.begin_path();
    ctx.arc(x + sway, y, 1.8f, 0, 7);
    ctx.fill();
  }
  for (std::size_t k = 0; k < game.decor_count(); ++k) {  // dandelion fluff drifting by
    const Decor& d = game.decor(k);
    ctx.set_stroke(rgb(0xFFFFFF, 0.75f));
    ctx.set_line_width(1);
    ctx.begin_path();
    for (int i = 0; i < 6; ++i) {
      const float a = i * kPi / 3 + d.ph * 0.5f;
      ctx.move_to(d.x, d.y);
      ctx.line_to(d.x + fcos(a) * d.r * 2, d.y + fsin(a) * d.r * 2);
    }
    ctx.stroke();
    ctx.set_fill(rgb(0xFFFFFF, 0.9f));
    ctx.begin_path();
    ctx.arc(d.x, d.y, d.r * 0.6f, 0, 7);
    ctx.fill();
  }
}

void draw_nest_back(Context& ctx) {
  ctx.set_fill(rgb(0x8A5A2E));
  ctx.begin_path();
  ctx.ellipse(kNestX, kNestY, kNestRx, 16, 0, 0, 7);
  ctx.fill();
  ctx.set_fill(rgb(0x6E4524));
  ctx.begin_path();
  ctx.ellipse(kNestX, kNestY - 2, kNestRx - 8, 11, 0, 0, 7);
  ctx.fill();
}

void draw_nest_front(Context& ctx) {
  const int g = ctx.create_linear_gradient(0, kNestY - 2, 0, kNestY + 24);
  ctx.add_color_stop(g, 0, rgb(0xE0AE6A));
  ctx.add_color_stop(g, 1, rgb(0xB47A3E));
  ctx.set_fill_gradient(g);
  ctx.begin_path();
  ctx.ellipse(kNestX, kNestY, kNestRx + 2, 24, 0, 0, kPi);
  ctx.ellipse(kNestX, kNestY, kNestRx + 2, 10, 0, kPi, 0, true);
  ctx.close_path();
  ctx.fill();
  ctx.set_stroke(rgb(0x9C6633));
  ctx.set_line_width(1.5f);
  ctx.stroke();
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_width(1.6f);
  const Straw* s = straws();
  for (int i = 0; i < 28; ++i) {  // woven straws
    const float a = s[i].u * kPi, x = kNestX + fcos(a) * kNestRx, y = kNestY + fsin(a) * s[i].ry;
    const float tilt = s[i].tilt;
    ctx.set_stroke(rgb(s[i].light ? 0xF2C98A : 0x9C6633));
    ctx.begin_path();
    ctx.move_to(x - 7, y - 1 + tilt * 6);
    ctx.quadratic_curve_to(x, y + 2, x + 7, y - 1 - tilt * 6);
    ctx.stroke();
  }
}

// ---------- creatures (body centre at 0,0; feet at y = +34) ----------
enum class Mood : uint8_t { IDLE, HAPPY, SQUINT, OPEN };

struct FaceSpec {
  float ey, sp, er, mx, my, mr;
};
constexpr FaceSpec kFaces[kCreatureCount] = {
    {-4, 11, 7, 0, 12, 5},        // BLOOP
    {-4, 11, 7, 0, 11, 5},        // FLUFFWING
    {-6, 9, 6.5f, 0, 10, 4.5f},     // SPROUTLING
    {-4, 11, 7, 0, 11, 5},        // ZIGZAG
    {-4, 11, 7, 0, 17, 4.5f},      // PUDDLEPUP
    {-12, 10, 6.5f, 0, 2, 4.5f},    // GLIMMERBUG
    {-6, 10, 7, 0, 8, 5},         // ROLYPOLE
    {-12, 10, 6.5f, -12, 16, 3.5f}, // SNORKLE
};

int body_grad(Context& ctx, const Swatch& c, float r) {
  const int g = ctx.create_radial_gradient(-10, -12, 4, 0, 0, r);
  ctx.add_color_stop(g, 0, rgb(c.light));
  ctx.add_color_stop(g, 0.55f, rgb(c.base));
  ctx.add_color_stop(g, 1, rgb(c.shadow));
  return g;
}

void finish_body(Context& ctx, const Swatch& c, float r) {
  ctx.set_fill_gradient(body_grad(ctx, c, r));
  ctx.fill();
  ctx.set_stroke(rgb(c.shadow));
  ctx.set_line_width(2);
  ctx.set_line_join(Join::ROUND);
  ctx.stroke();
}

void shine(Context& ctx, float x, float y) {
  ctx.set_fill(rgb(0xFFFFFF, 0.6f));
  ctx.begin_path();
  ctx.ellipse(x, y, 7, 4, -0.6f, 0, 7);
  ctx.fill();
  ctx.begin_path();
  ctx.arc(x + 8, y - 5, 1.6f, 0, 7);
  ctx.fill();
}

void face(Context& ctx, const FaceSpec& o, Mood mood, float look_x, float look_y, float blink) {
  float ln = std::hypot(static_cast<float>(look_x), static_cast<float>(look_y));
  if (ln == 0) ln = 1;
  const float lx = look_x / ln * o.er * 0.35f, ly = look_y / ln * o.er * 0.35f;
  ctx.set_line_cap(Cap::ROUND);
  for (const int s : {-1, 1}) {
    const float ex = s * o.sp;
    if (mood == Mood::HAPPY || mood == Mood::SQUINT) {
      ctx.set_stroke(rgb(kInk));
      ctx.set_line_width(2.2f);
      ctx.begin_path();
      if (mood == Mood::HAPPY) {
        ctx.arc(ex, o.ey + 2, o.er * 0.75f, 1.15f * kPi, 1.85f * kPi);
      } else {
        ctx.move_to(ex - o.er * 0.7f, o.ey);
        ctx.line_to(ex + o.er * 0.7f, o.ey);
      }
      ctx.stroke();
      continue;
    }
    if (blink < 0) {
      ctx.set_stroke(rgb(kInk));
      ctx.set_line_width(2);
      ctx.begin_path();
      ctx.move_to(ex - o.er * 0.7f, o.ey);
      ctx.line_to(ex + o.er * 0.7f, o.ey);
      ctx.stroke();
      continue;
    }
    ctx.set_fill(rgb(0xFFFFFF));
    ctx.begin_path();
    ctx.arc(ex, o.ey, o.er, 0, 7);
    ctx.fill();
    ctx.set_fill(rgb(kInk));
    ctx.begin_path();
    ctx.arc(ex + lx, o.ey + ly, o.er * 0.5f, 0, 7);
    ctx.fill();
    ctx.set_fill(rgb(0xFFFFFF));
    ctx.begin_path();
    ctx.arc(ex + lx - o.er * 0.18f, o.ey + ly - o.er * 0.2f, o.er * 0.17f, 0, 7);
    ctx.fill();
  }
  ctx.set_fill(rgb(0xF06BB8, 0.45f));
  ctx.begin_path();
  ctx.arc(-(o.sp + o.er), o.ey + o.er + 2, 3.5f, 0, 7);
  ctx.arc(o.sp + o.er, o.ey + o.er + 2, 3.5f, 0, 7);
  ctx.fill();
  ctx.set_stroke(rgb(kInk));
  ctx.set_fill(rgb(kInk));
  ctx.set_line_width(2);
  if (mood == Mood::OPEN || mood == Mood::SQUINT) {
    ctx.begin_path();
    ctx.ellipse(o.mx, o.my + 1, o.mr * 0.7f, o.mr * 0.8f, 0, 0, 7);
    ctx.fill();
  } else {
    ctx.begin_path();
    ctx.arc(o.mx, o.my - 2, mood == Mood::HAPPY ? o.mr * 1.3f : o.mr, 0.15f * kPi, 0.85f * kPi);
    ctx.stroke();
  }
}

void draw_body(Context& ctx, int k, const Swatch& c, float time) {
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_join(Join::ROUND);
  if (k == 0) {  // BLOOP: blob + curly antenna
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.move_to(2, -26);
    ctx.quadratic_curve_to(14, -40, 6, -46);
    ctx.stroke();
    ctx.set_fill(rgb(c.light));
    ctx.begin_path();
    ctx.arc(5, -48, 5, 0, 7);
    ctx.fill();
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(2);
    ctx.stroke();
    ctx.begin_path();
    ctx.ellipse(0, 2, 36, 31, 0, 0, 7);
    finish_body(ctx, c, 36);
  } else if (k == 1) {  // FLUFFWING: puffball + tiny wings
    const float flap = fsin(time * 14) * 0.25f;
    for (const int s : {-1, 1}) {
      ctx.save();
      ctx.translate(s * 30, -6);
      ctx.rotate(s * (0.5f + flap));
      ctx.set_fill(rgb(c.light));
      ctx.begin_path();
      ctx.ellipse(s * 8, 0, 12, 7, 0, 0, 7);
      ctx.fill();
      ctx.set_stroke(rgb(c.shadow));
      ctx.set_line_width(2);
      ctx.stroke();
      ctx.restore();
    }
    ctx.begin_path();
    for (int i = 0; i < 48; ++i) {
      const float a = i / 48.0f * kPi * 2, r = 32 + 2.2f * fcos(a * 12);
      ctx.line_to(fcos(a) * r, 2 + fsin(a) * r);
    }
    ctx.close_path();
    finish_body(ctx, c, 34);
    ctx.set_fill(rgb(c.base));
    ctx.begin_path();
    ctx.arc(-5, -31, 4, 0, 7);
    ctx.arc(0, -34, 5, 0, 7);
    ctx.arc(5, -31, 4, 0, 7);
    ctx.fill();
  } else if (k == 2) {  // SPROUTLING: bean + leaf
    ctx.set_stroke(rgb(0x3A9442));
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.move_to(0, -31);
    ctx.line_to(1, -42);
    ctx.stroke();
    ctx.save();
    ctx.translate(9, -46);
    ctx.rotate(-0.5f + fsin(time * 2) * 0.1f);
    ctx.set_fill(rgb(0x2DB26E));
    leaf_path(ctx, 9);
    ctx.fill();
    ctx.restore();
    ctx.save();
    ctx.translate(-6, -43);
    ctx.rotate(kPi + 0.4f);
    ctx.set_fill(rgb(0x2DB26E));
    leaf_path(ctx, 6);
    ctx.fill();
    ctx.restore();
    ctx.begin_path();
    ctx.ellipse(0, 2, 28, 34, 0, 0, 7);
    finish_body(ctx, c, 36);
  } else if (k == 3) {  // ZIGZAG: lizard + zigzag tail + spikes
    constexpr float tail[5][2] = {{24, 16}, {36, 4}, {42, 16}, {50, 2}, {57, 12}};
    const float widths[2] = {9, 6};
    const uint32_t cols[2] = {c.shadow, c.base};
    for (int j = 0; j < 2; ++j) {
      ctx.set_stroke(rgb(cols[j]));
      ctx.set_line_width(widths[j]);
      ctx.begin_path();
      for (int i = 0; i < 5; ++i) {
        if (i) ctx.line_to(tail[i][0], tail[i][1]);
        else ctx.move_to(tail[i][0], tail[i][1]);
      }
      ctx.stroke();
    }
    ctx.set_fill(rgb(c.light));
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(2);
    constexpr float spikes[3][3] = {{-18, -11, -4}, {-6, 1, 8}, {6, 13, 20}};
    for (const auto& s : spikes) {
      ctx.begin_path();
      ctx.move_to(s[0], -20);
      ctx.line_to(s[1], -35);
      ctx.line_to(s[2], -20);
      ctx.close_path();
      ctx.fill();
      ctx.stroke();
    }
    ctx.begin_path();
    ctx.ellipse(0, 4, 34, 28, 0, 0, 7);
    finish_body(ctx, c, 34);
  } else if (k == 4) {  // PUDDLEPUP: droplet + floppy ears
    ctx.begin_path();
    ctx.move_to(0, -38);
    ctx.bezier_curve_to(10, -30, 34, -14, 34, 8);
    ctx.arc(0, 8, 34, 0, kPi);
    ctx.bezier_curve_to(-34, -14, -10, -30, 0, -38);
    ctx.close_path();
    finish_body(ctx, c, 40);
    const float sw = fsin(time * 3) * 0.08f;
    for (const int s : {-1, 1}) {
      ctx.save();
      ctx.translate(s * 28, -8);
      ctx.rotate(s * (0.35f + sw));
      ctx.set_fill(rgb(c.shadow));
      ctx.begin_path();
      ctx.ellipse(0, 6, 9, 17, 0, 0, 7);
      ctx.fill();
      ctx.restore();
    }
    ctx.set_fill(rgb(kInk));
    ctx.begin_path();
    ctx.ellipse(0, 8, 4, 3, 0, 0, 7);
    ctx.fill();
  } else if (k == 5) {  // GLIMMERBUG: dotted shell + antennae
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(2);
    for (const int s : {-1, 1}) {
      ctx.begin_path();
      ctx.move_to(s * 8, -26);
      ctx.quadratic_curve_to(s * 10, -40, s * 17, -45);
      ctx.stroke();
      ctx.set_fill(rgb(c.shadow));
      ctx.begin_path();
      ctx.arc(s * 17, -45, 3.5f, 0, 7);
      ctx.fill();
    }
    ctx.begin_path();
    ctx.ellipse(0, 2, 36, 30, 0, 0, 7);
    finish_body(ctx, c, 36);
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(2);
    ctx.begin_path();
    ctx.move_to(0, 9);
    ctx.line_to(0, 31);
    ctx.stroke();
    ctx.set_fill(rgb(kInk, 0.55f));
    constexpr float dots[6][3] = {{-18, 14, 4.5f}, {18, 14, 4.5f}, {-10, 24, 3.5f},
                                   {10, 24, 3.5f},  {-27, 0, 3},   {27, 0, 3}};
    for (const auto& d : dots) {
      ctx.begin_path();
      ctx.arc(d[0], d[1], d[2], 0, 7);
      ctx.fill();
    }
  } else if (k == 6) {  // ROLYPOLE: striped ball
    ctx.begin_path();
    ctx.arc(0, 0, 34, 0, 7);
    finish_body(ctx, c, 34);
    ctx.set_stroke(rgb(0xC2621B, 0.5f));
    ctx.set_line_width(3.5f);
    ctx.set_line_cap(Cap::BUTT);
    for (const float y : {-24.0f, 16.0f, 25.0f}) {
      const float w = std::sqrt(static_cast<float>(34 * 34 - y * y)) - 2;
      ctx.begin_path();
      ctx.move_to(-w, y);
      ctx.quadratic_curve_to(0, y + 4, w, y);
      ctx.stroke();
    }
    ctx.set_line_cap(Cap::ROUND);
  } else {  // SNORKLE: curly snout
    ctx.set_fill(rgb(c.base));
    for (const int s : {-1, 1}) {
      ctx.begin_path();
      ctx.arc(s * 18, -26, 7, 0, 7);
      ctx.fill();
      ctx.set_stroke(rgb(c.shadow));
      ctx.set_line_width(2);
      ctx.stroke();
    }
    ctx.begin_path();
    ctx.ellipse(0, 2, 32, 32, 0, 0, 7);
    finish_body(ctx, c, 34);
  }
  if (k == 7) {
    const float widths[2] = {11, 7};
    const uint32_t cols[2] = {c.shadow, c.light};
    for (int j = 0; j < 2; ++j) {
      ctx.set_stroke(rgb(cols[j]));
      ctx.set_line_width(widths[j]);
      ctx.begin_path();
      ctx.move_to(0, 2);
      ctx.bezier_curve_to(14, 8, 26, 2, 30, -8);
      ctx.stroke();
    }
    ctx.set_fill(rgb(c.light));
    ctx.begin_path();
    ctx.arc(30, -10, 6, 0, 7);
    ctx.fill();
    ctx.set_stroke(rgb(c.shadow));
    ctx.set_line_width(2);
    ctx.stroke();
    ctx.set_fill(rgb(c.shadow));
    ctx.begin_path();
    ctx.arc(28, -11, 1.4f, 0, 7);
    ctx.arc(32, -9, 1.4f, 0, 7);
    ctx.fill();
  }
  shine(ctx, k == 5 ? -14.f : -12.f, k == 5 ? -10.f : -14.f);
}

// (x, y) = body centre at scale s; pivot for squash/rotation is the feet.
void draw_creature(Context& ctx, Creature kind, float x, float y, float s, float q, float rot, Mood mood,
                   float look_x, float look_y, float blink, float time) {
  const int k = static_cast<int>(kind);
  const Swatch c = swatch(kind);
  ctx.save();
  ctx.set_global_alpha(1);
  ctx.translate(x, y + 34 * s);
  ctx.rotate(rot);
  ctx.scale(s * (1 - q), s * (1 + q));
  ctx.translate(0, -34);
  ctx.set_fill(rgb(kInk, 0.15f));
  ctx.begin_path();
  ctx.ellipse(2, 35, 26, 5, 0, 0, 7);
  ctx.fill();
  draw_body(ctx, k, c, time);
  face(ctx, kFaces[k], mood, look_x, look_y, blink);
  ctx.restore();
}

// ---------- scene pieces ----------
void draw_egg(Context& ctx, const HatchAPetGame& game) {
  const Phase ph = game.phase();
  if (ph != Phase::INTRO && ph != Phase::EGG && ph != Phase::PRE_HATCH && ph != Phase::ROLL) return;
  const Egg& egg = game.egg();
  const Swatch c = swatch(egg.creature);
  const float time = static_cast<float>(game.time());
  const float br = fsin(time * 2 * kPi / 2.8f) * 0.01f;
  ctx.save();
  ctx.translate(egg.x, kEggY + kEggRy + egg.y_off);
  ctx.rotate(egg.ang);
  ctx.scale((1 - egg.q) * (1 + br * 0.5f), (1 + egg.q) * (1 + br));
  ctx.translate(0, -kEggRy);
  ctx.rotate(egg.roll);
  if (ph == Phase::EGG && game.hint() >= 1) {
    ctx.set_stroke(rgb(kHintGlow));
    ctx.set_line_width(6);
    ctx.set_global_alpha(0.35f + 0.4f * (0.5f + 0.5f * fsin(time * 2 * kPi / kGlowPeriod)));
    egg_path(ctx, kEggRx + 6, kEggRy + 6);
    ctx.stroke();
    ctx.set_global_alpha(1);
  }
  shell_fill(ctx, Part::WHOLE);
  draw_decals(ctx, egg.pattern, c.base, Part::WHOLE);
  shell_shine(ctx);
  draw_crack(ctx, egg, static_cast<float>(egg.cracks) / egg.need, ph == Phase::PRE_HATCH);
  ctx.restore();
}

void draw_half(Context& ctx, const HatchAPetGame& game, bool top) {
  const Swatch c = swatch(game.egg().creature);
  const Halves& h = game.halves();
  const float a = top ? 1 - clamp(static_cast<float>(h.t - 0.6) / 0.6f, 0, 1) : 1 - clamp(static_cast<float>(h.t - 0.5) / 0.7f, 0, 1);
  if (a <= 0) return;
  ctx.save();
  ctx.set_global_alpha(a);
  ctx.translate(kEggX, kEggY);
  if (top) {
    ctx.translate(h.top_x, h.top_y + kCrackY);
    ctx.rotate(h.top_a);
    ctx.translate(0, -kCrackY);
  } else {
    ctx.translate(0, h.bot_y);
  }
  shell_fill(ctx, top ? Part::TOP : Part::BOT);
  draw_decals(ctx, game.egg().pattern, c.base, top ? Part::TOP : Part::BOT);
  if (top) shell_shine(ctx);
  ctx.restore();
}

void draw_pet(Context& ctx, const HatchAPetGame& game) {
  const Phase ph = game.phase();
  if (ph != Phase::HATCH && ph != Phase::PET && ph != Phase::BYE) return;
  const float st_t = static_cast<float>(game.phase_time());
  if (ph == Phase::HATCH && st_t < 0.05f) return;
  const Pet& pet = game.pet();
  // petPose()
  float yo = 0, rot = 0;
  Mood mood = Mood::IDLE;
  if (ph == Phase::HATCH) mood = st_t > 0.25f ? Mood::HAPPY : Mood::OPEN;
  if (pet.react == 0) {
    rot = fsin(static_cast<float>(pet.rt) * 40) * 0.12f;
    mood = Mood::HAPPY;
  } else if (pet.react == 1) {
    const float k = clamp((0.55f - static_cast<float>(pet.rt)) / 0.43f, 0, 1);
    yo = -fsin(kPi * k) * 26;
    mood = Mood::OPEN;
  } else if (pet.react == 2) {
    mood = pet.rt > 0.45f ? Mood::SQUINT : Mood::HAPPY;
    rot = pet.rt > 0.45f ? -0.08f * clamp((0.8f - static_cast<float>(pet.rt)) / 0.35f, 0, 1) : 0;
  }
  if (ph == Phase::BYE) {
    mood = Mood::HAPPY;
    rot = fsin(st_t * 18) * 0.1f;
  }
  draw_creature(ctx, game.egg().creature, pet.x, pet.y + yo, pet.s, pet.q, rot, mood, pet.look_x, pet.look_y,
                static_cast<float>(pet.blink), static_cast<float>(game.time()));
}

void draw_shelf(Context& ctx, const HatchAPetGame& game) {
  for (std::size_t i = 0; i < game.shelf_count(); ++i) {
    const Friend& f = game.shelf(i);
    const float bob = fsin(f.ph * 2.1f) * 1, rot = f.wave > 0 ? fsin(f.wave * 30) * 0.15f : 0;
    draw_creature(ctx, f.c, f.x, f.y + bob, HatchAPetGame::kMini, f.q, rot, f.wave > 0 ? Mood::HAPPY : Mood::IDLE,
                  0, 1, 1, static_cast<float>(game.time()));
  }
}

void choice_bubble(Context& ctx, float x, float y, float s, uint32_t light, uint32_t base, uint32_t shadow,
                   float glow, bool yes) {
  ctx.save();
  ctx.translate(x, y);
  ctx.scale(s, s);
  if (glow > 0) {
    ctx.set_stroke(rgb(kHintGlow));
    ctx.set_global_alpha(0.35f + 0.5f * glow);
    ctx.set_line_width(6);
    ctx.begin_path();
    ctx.arc(0, 0, kChoiceR + 6, 0, 7);
    ctx.stroke();
    ctx.set_global_alpha(1);
  }
  ctx.set_fill(rgb(kInk, 0.15f));
  ctx.begin_path();
  ctx.ellipse(2, kChoiceR + 6, kChoiceR * 0.6f, 4, 0, 0, 7);
  ctx.fill();
  const int g = ctx.create_radial_gradient(-9, -11, 4, 0, 0, kChoiceR);
  ctx.add_color_stop(g, 0, rgb(light));
  ctx.add_color_stop(g, 0.55f, rgb(base));
  ctx.add_color_stop(g, 1, rgb(shadow));
  ctx.set_fill_gradient(g);
  ctx.begin_path();
  ctx.arc(0, 0, kChoiceR, 0, 7);
  ctx.fill();
  ctx.set_stroke(rgb(shadow));
  ctx.set_line_width(2);
  ctx.stroke();
  if (!yes) {
    ctx.set_fill(rgb(0xFFFFFF, 0.95f));
    heart_path(ctx, 0, 2, 13);
    ctx.fill();
  } else {
    ctx.save();
    ctx.translate(0, 2);
    ctx.set_fill(rgb(0xFFFFFF));
    egg_path(ctx, 11, 14);
    ctx.fill();
    ctx.set_fill(rgb(0x8EE0B4));
    constexpr float dots[3][3] = {{-4, -5, 2.5f}, {4, 2, 3}, {-3, 8, 2}};
    for (const auto& d : dots) {
      ctx.begin_path();
      ctx.arc(d[0], d[1], d[2], 0, 7);
      ctx.fill();
    }
    ctx.restore();
    ctx.set_fill(rgb(0xFFEB9C));
    star_path(ctx, 13, -12, 5, 2.2f, -kPi / 2);
    ctx.fill();
  }
  ctx.set_fill(rgb(0xFFFFFF, 0.75f));
  ctx.begin_path();
  ctx.ellipse(-10, -13, 6, 3.5f, -0.6f, 0, 7);
  ctx.fill();
  ctx.restore();
}

void draw_choices(Context& ctx, const HatchAPetGame& game) {
  const Choices& ch = game.choices();
  const bool showing = ch.show, fading = !showing && ch.out > 0;
  if (!showing && !fading) return;
  const float time = static_cast<float>(game.time());
  const float k = showing ? ease_out_back(clamp(static_cast<float>(ch.t) / 0.5f, 0, 1)) : 1;
  const float s = showing ? 1 : static_cast<float>(ch.out) / 0.35f, rise = (1 - k) * 90;
  const float glow = showing && ch.idle > kIdleVisualHint ? 0.5f + 0.5f * fsin(time * 2 * kPi / kGlowPeriod) : 0;
  auto bob = [time](int i) { return fsin(time * 2.1f + i * 1.7f) * kBob; };
  choice_bubble(ctx, HatchAPetGame::kNoX, HatchAPetGame::kNoY + rise + bob(0), s, 0xFFB3DD, 0xF06BB8, 0xB8448A, glow,
                false);
  choice_bubble(ctx, HatchAPetGame::kYesX, HatchAPetGame::kYesY + rise + bob(1), s, 0x8EE0B4, 0x2DB26E, 0x1E8452,
                glow, true);
}

void draw_fx(Context& ctx, const HatchAPetGame& game) {
  for (std::size_t i = 0; i < game.ring_count(); ++i) {
    const RingFx& r = game.ring(i);
    const float k = r.t / r.dur;
    ctx.set_global_alpha(1 - k);
    ctx.set_stroke(rgb(r.color));
    ctx.set_line_width(r.w);
    ctx.begin_path();
    ctx.arc(r.x, r.y, lerp(r.r0, r.r1, ease_out_cubic(k)), 0, 7);
    ctx.stroke();
  }
  for (std::size_t i = 0; i < game.particle_count(); ++i) {
    const Particle& p = game.particle(i);
    ctx.set_global_alpha(clamp(p.life / p.max * 1.6f, 0, 1));
    ctx.set_composite(p.add ? Op::LIGHTER : Op::SOURCE_OVER);
    ctx.set_fill(rgb(p.color));
    if (p.shape == ParticleShape::STAR) {
      star_path(ctx, p.x, p.y, p.size, p.size * 0.45f, p.rot);
      ctx.fill();
    } else if (p.shape == ParticleShape::RECT) {
      ctx.save();
      ctx.translate(p.x, p.y);
      ctx.rotate(p.rot);
      ctx.fill_rect(-p.size / 2, -p.size / 4, p.size, p.size / 2);
      ctx.restore();
    } else if (p.shape == ParticleShape::HEART) {
      heart_path(ctx, p.x, p.y, p.size);
      ctx.fill();
    } else {
      ctx.begin_path();
      ctx.arc(p.x, p.y, p.size * (0.5f + 0.5f * p.life / p.max), 0, 7);
      ctx.fill();
    }
  }
  ctx.set_global_alpha(1);
  ctx.set_composite(Op::SOURCE_OVER);
}

void draw_helper_hand(Context& ctx, const HatchAPetGame& game) {
  if (game.phase() != Phase::EGG || game.hint() < 2) return;
  const float y = kEggY + 24 + std::fabs(fsin(static_cast<float>(game.time()) * 4)) * -6;
  ctx.save();
  ctx.translate(kEggX + 30, y);
  ctx.rotate(-0.5f);
  ctx.set_fill(rgb(0xFFFFFF));
  ctx.set_stroke(rgb(kInk));
  ctx.set_line_width(1.5f);
  round_rect(ctx, -4, -16, 8, 18, 4);
  ctx.fill();
  ctx.stroke();
  ctx.begin_path();
  ctx.ellipse(0, 6, 9, 8, 0, 0, 7);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

void draw_chrome(Context& ctx, bool mic_enabled) {
  ctx.set_fill(rgb(kCream));  // Home
  round_rect(ctx, 6, 6, 44, 44, 14);
  ctx.fill();
  ctx.set_stroke(rgb(0xD9C7A3));
  ctx.set_line_width(2);
  ctx.stroke();
  ctx.set_fill(rgb(kInk));
  ctx.begin_path();
  ctx.move_to(28, 15);
  ctx.line_to(41, 27);
  ctx.line_to(37, 27);
  ctx.line_to(37, 39);
  ctx.line_to(19, 39);
  ctx.line_to(19, 27);
  ctx.line_to(15, 27);
  ctx.close_path();
  ctx.fill();
  ctx.set_global_alpha(mic_enabled ? 1.0f : 0.45f);  // Mic (faded when voice is off)
  ctx.set_fill(rgb(kCream));
  round_rect(ctx, 270, 6, 44, 44, 14);
  ctx.fill();
  ctx.set_fill(rgb(kInk));
  round_rect(ctx, 287, 14, 10, 18, 5);
  ctx.fill();
  ctx.set_stroke(rgb(kInk));
  ctx.set_line_width(2);
  ctx.begin_path();
  ctx.arc(292, 25, 9, 0.1f * kPi, 0.9f * kPi);
  ctx.stroke();
  ctx.begin_path();
  ctx.move_to(292, 34);
  ctx.line_to(292, 40);
  ctx.stroke();
  ctx.set_global_alpha(1);
}

void draw_badge(Context& ctx, const HatchAPetGame& game) {
  const Phase ph = game.phase();
  if (ph == Phase::START || ph == Phase::INTRO) return;
  const float time = static_cast<float>(game.time());
  const Swatch c = swatch(game.egg().creature);
  const float bob = fsin(time * 2.2f) * 1.5f;
  const float press = 1 + clamp(-game.badge().sv * 0.012f, -0.1f, 0.12f);
  ctx.save();
  ctx.translate(160, 32 + bob);
  ctx.scale(press, 2 - press);
  if (game.hint() >= 1 && ph == Phase::EGG) {
    ctx.set_stroke(rgb(kHintGlow));
    ctx.set_line_width(4);
    ctx.set_global_alpha(0.4f + 0.4f * fsin(time * 2 * kPi / kGlowPeriod));
    ctx.begin_path();
    ctx.arc(0, 0, 29, 0, 7);
    ctx.stroke();
    ctx.set_global_alpha(1);
  }
  ctx.set_fill(rgb(kCream));
  ctx.begin_path();
  ctx.arc(0, 0, 26, 0, 7);
  ctx.fill();
  ctx.set_stroke(rgb(0xD9C7A3));
  ctx.set_line_width(2);
  ctx.stroke();
  const bool eggish = ph == Phase::EGG || ph == Phase::PRE_HATCH || ph == Phase::ROLL;
  if (game.badge().happy > 0) {
    ctx.set_fill(rgb(c.base));
    ctx.begin_path();
    ctx.arc(0, 0, 20, 0, 7);
    ctx.fill();
    ctx.set_fill(rgb(0xFFFFFF));
    ctx.begin_path();
    ctx.arc(-7, -4, 2.5f, 0, 7);
    ctx.arc(7, -4, 2.5f, 0, 7);
    ctx.fill();
    ctx.set_stroke(rgb(0xFFFFFF));
    ctx.set_line_width(2.5f);
    ctx.begin_path();
    ctx.arc(0, 2, 8, 0.15f * kPi, 0.85f * kPi);
    ctx.stroke();
  } else if (eggish) {
    ctx.save();
    ctx.rotate(fsin(time * 3) * 0.08f);
    ctx.translate(0, 1);
    ctx.set_fill(rgb(0xFFFFFF));
    egg_path(ctx, 13, 17);
    ctx.fill();
    ctx.set_stroke(rgb(kShellLine));
    ctx.set_line_width(1.5f);
    ctx.stroke();
    ctx.set_fill(rgb(c.base));
    constexpr float dots[3][3] = {{-4, -6, 2.6f}, {5, 0, 3}, {-3, 8, 2.4f}};
    for (const auto& d : dots) {
      ctx.begin_path();
      ctx.arc(d[0], d[1], d[2], 0, 7);
      ctx.fill();
    }
    ctx.restore();
  } else {
    ctx.set_fill(rgb(c.base));
    heart_path(ctx, 0, 2, 13);
    ctx.fill();
  }
  ctx.restore();
}

void draw_start(Context& ctx, float time) {
  ctx.set_fill(rgb(0x1E2245, 0.35f));
  ctx.fill_rect(0, 0, W, H);
  const float p = 1 + fsin(time * 3) * 0.04f;
  ctx.save();
  ctx.translate(160, 120);
  ctx.scale(p, p);
  ctx.set_fill(rgb(kCream));
  ctx.begin_path();
  ctx.arc(0, 0, 36, 0, 7);
  ctx.fill();
  ctx.set_fill(rgb(0x2DB26E));
  ctx.begin_path();
  ctx.move_to(-10, -16);
  ctx.line_to(18, 0);
  ctx.line_to(-10, 16);
  ctx.close_path();
  ctx.fill();
  ctx.restore();
}

}  // namespace

void paint_background(Context& ctx) {
  ctx.set_flatten_tolerance(kFlatTol);
  int g = ctx.create_linear_gradient(0, 0, 0, 130);
  ctx.add_color_stop(g, 0, rgb(kSkyTop));
  ctx.add_color_stop(g, 1, rgb(kSkyBot));
  ctx.set_fill_gradient(g);
  ctx.fill_rect(0, 0, W, 130);
  ctx.set_fill(rgb(0xFFE066, 0.35f));  // sun
  ctx.begin_path();
  ctx.arc(228, 38, 21, 0, 7);
  ctx.fill();
  ctx.set_fill(rgb(0xFFE58A));
  ctx.begin_path();
  ctx.arc(228, 38, 14, 0, 7);
  ctx.fill();
  ctx.set_fill(rgb(kHillFar));  // far hills
  ctx.begin_path();
  ctx.move_to(0, H);
  for (int x = 0; x <= W; x += 8) ctx.line_to(x, 114 + fsin(x / 37.0f) * 6 + fsin(x / 13.0f) * 1.5f);
  ctx.line_to(W, H);
  ctx.close_path();
  ctx.fill();
  g = ctx.create_linear_gradient(0, 140, 0, 240);
  ctx.add_color_stop(g, 0, rgb(kHillNear));
  ctx.add_color_stop(g, 1, rgb(kGrassBot));
  ctx.set_fill_gradient(g);
  ctx.begin_path();  // near meadow
  ctx.move_to(0, H);
  for (int x = 0; x <= W; x += 8) ctx.line_to(x, 148 + fsin(x / 53.0f + 1) * 7);
  ctx.line_to(W, H);
  ctx.close_path();
  ctx.fill();
  // Steady-state canvas line state (round joins carry over from the egg and
  // creature drawing of earlier frames).
  ctx.set_stroke(rgb(0x2E783C, 0.55f));
  ctx.set_line_width(1.5f);
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_join(Join::ROUND);
  const Tuft* t = tufts();
  for (int i = 0; i < 22; ++i) {
    const float x = t[i].x, y = t[i].y;
    ctx.begin_path();
    ctx.move_to(x - 3, y - 4);
    ctx.line_to(x, y);
    ctx.line_to(x + 3, y - 5);
    ctx.stroke();
  }
}

void paint_frame(Context& ctx, const HatchAPetGame& game, bool mic_enabled) {
  // Coarser curve flattening than the 0.05 px default: same look within the
  // golden thresholds, far fewer points to record and rasterise per frame.
  ctx.set_flatten_tolerance(kFlatTol);
  // Canvas state persists across reference frames; after the tufts the cap
  // is round, and joins are round once an egg or creature has been drawn.
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_join(Join::ROUND);
  draw_background(ctx, game);
  draw_shelf(ctx, game);
  draw_nest_back(ctx);
  if (game.halves().on) draw_half(ctx, game, false);
  draw_egg(ctx, game);
  draw_nest_front(ctx);
  draw_pet(ctx, game);
  if (game.halves().on) draw_half(ctx, game, true);
  draw_choices(ctx, game);
  draw_helper_hand(ctx, game);
  draw_fx(ctx, game);
  draw_badge(ctx, game);
  draw_chrome(ctx, mic_enabled);
  if (game.phase() == Phase::START) draw_start(ctx, static_cast<float>(game.time()));
}

}  // namespace little_arcade::hatch_a_pet
