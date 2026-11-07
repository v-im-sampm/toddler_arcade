#include "color_pop_scene.hpp"

#include <cmath>
#include <initializer_list>

#include "color_pop_sprites.hpp"
#include "graphics/Color.h"
#include "graphics/Renderer.h"
#include "input/TouchEvent.h"
#include "little_arcade/games/color_pop_stage.hpp"

namespace little_arcade::color_pop::pr32 {

namespace pg = pixelroot32::graphics;
namespace sp = little_arcade::color_pop::sprites;

namespace {
constexpr float kPi = 3.14159265f;
constexpr float R = static_cast<float>(ColorPopGame::R);
constexpr uint32_t kInk = 0x3B3550, kCream = 0xFFF3D6, kHintGlow = 0xFFE066, kWhite = 0xFFFFFF,
                   kBorder = 0xD9C7A3;
constexpr uint32_t kSkyTop = 0xBFE6FF, kSkyBot = 0xE6F5FF, kSeaTop = 0xA9DBFB, kSeaBot = 0x6FBDF2,
                   kSand = 0xF3DDB0, kSandShadow = 0xDDB892;
constexpr uint32_t kCrabBody = 0xFF8A2A, kCrabDark = 0xC2621B, kCheek = 0xF06BB8;
constexpr uint8_t kSpriteSlot = 1;
constexpr float kGlowPeriod = 0.9f;

// The engine keeps palette pointers, so these tables are rewritten in place:
// g_prim[0] is the colour of the next primitive (drawn as Color index 0),
// g_spr is the 16-entry table of the next 4bpp sprite (sprite slot 1).
uint16_t g_prim[16];
uint16_t g_spr[16];

float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float ease_out_cubic(float t) {
  const float u = 1 - t;
  return 1 - u * u * u;
}
int iround(float v) { return static_cast<int>(std::lround(v)); }
float f(double v) { return static_cast<float>(v); }

constexpr uint16_t rgb565(uint32_t c) {
  return static_cast<uint16_t>(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = clampf(t, 0, 1);
  uint32_t out = 0;
  for (int s = 0; s <= 16; s += 8) {
    const float x = static_cast<float>((a >> s) & 0xFF), y = static_cast<float>((b >> s) & 0xFF);
    out |= static_cast<uint32_t>(x + (y - x) * t + 0.5f) << s;
  }
  return out;
}

// The static backdrop colour at row y: translucent paint is pre-mixed with it.
uint32_t backdrop(float y) {
  if (y < 64) return mix(kSkyTop, kSkyBot, y / 70.0f);
  if (y < 204) return mix(kSeaTop, kSeaBot, (y - 64) / 141.0f);
  return kSand;
}

void spr(int index, uint32_t rgb) { g_spr[index] = rgb565(rgb); }

class Painter {
 public:
  explicit Painter(pg::Renderer& r) : r_(r) {}

  void rect(int x, int y, int w, int h, uint16_t c) {
    if (w > 0 && h > 0) r_.drawFilledRectangleW(x, y, w, h, c);
  }
  // Opaque filled circle (engine primitive).
  void disc(float x, float y, float rad, uint16_t c) {
    g_prim[0] = c;
    const int ri = rad < 0.5f ? 0 : iround(rad - 0.5f);
    r_.drawFilledCircle(iround(x), iround(y), ri, pg::Color::Black);
  }
  void dot(float x, float y, float w, uint16_t c) {
    if (w < 1.75f) {
      rect(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), 1, 1, c);
    } else if (w < 2.75f) {
      rect(iround(x - 1), iround(y - 1), 2, 2, c);
    } else {
      disc(x, y, w * 0.5f, c);
    }
  }
  void line(float x0, float y0, float x1, float y1, float w, uint16_t c) {
    const float len = std::fmax(std::fabs(x1 - x0), std::fabs(y1 - y0));
    const int n = len < 1 ? 1 : static_cast<int>(std::ceil(len));
    for (int i = 0; i <= n; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(n);
      dot(lerpf(x0, x1, t), lerpf(y0, y1, t), w, c);
    }
  }
  void quad(float x0, float y0, float cx, float cy, float x1, float y1, float w, uint16_t c) {
    constexpr int kSeg = 10;
    float px = x0, py = y0;
    for (int i = 1; i <= kSeg; ++i) {
      const float t = static_cast<float>(i) / kSeg, u = 1 - t;
      const float x = u * u * x0 + 2 * u * t * cx + t * t * x1, y = u * u * y0 + 2 * u * t * cy + t * t * y1;
      line(px, py, x, y, w, c);
      px = x;
      py = y;
    }
  }
  void arc(float cx, float cy, float rad, float a0, float a1, float w, uint16_t c) {
    const int n = static_cast<int>(std::ceil((a1 - a0) * rad * 0.5f)) + 2;
    float px = cx + std::cos(a0) * rad, py = cy + std::sin(a0) * rad;
    for (int i = 1; i <= n; ++i) {
      const float a = lerpf(a0, a1, static_cast<float>(i) / static_cast<float>(n));
      const float x = cx + std::cos(a) * rad, y = cy + std::sin(a) * rad;
      line(px, py, x, y, w, c);
      px = x;
      py = y;
    }
  }
  // Annulus ri < d < ro around (cx, cy) (a disc when ri <= 0), one engine
  // rectangle per row span; each row is mixed with that row's backdrop.
  void annulus(float cx, float cy, float ro, float ri, uint32_t rgb, float alpha) {
    if (ro <= 0 || alpha <= 0.02f) return;
    const bool opaque = alpha >= 0.99f;
    const uint16_t solid = rgb565(rgb);
    const int y0 = static_cast<int>(std::floor(cy - ro)), y1 = static_cast<int>(std::ceil(cy + ro));
    for (int y = y0; y <= y1; ++y) {
      const float dy = (static_cast<float>(y) + 0.5f) - cy;
      const float o2 = ro * ro - dy * dy;
      if (o2 <= 0) continue;
      const float xo = std::sqrt(o2);
      const uint16_t c = opaque ? solid : rgb565(mix(backdrop(static_cast<float>(y)), rgb, alpha));
      const float i2 = ri * ri - dy * dy;
      if (ri <= 0 || i2 <= 0) {
        span(cx - xo, cx + xo, y, c);
      } else {
        const float xi = std::sqrt(i2);
        span(cx - xo, cx - xi, y, c);
        span(cx + xi, cx + xo, y, c);
      }
    }
  }
  void ring(float cx, float cy, float rad, float w, uint32_t rgb, float alpha) {
    annulus(cx, cy, rad + w * 0.5f, rad - w * 0.5f, rgb, alpha);
  }
  // Baked 4bpp art at an anchor, coloured by g_spr.
  void art(const sp::Art& a, float ax, float ay) {
    r_.drawSprite(a.sprite, iround(ax) - a.ox, iround(ay) - a.oy, kSpriteSlot);
  }
  // 1bpp star of outer radius r centred at (x, y).
  void star(int r, float x, float y, uint16_t c) {
    g_prim[0] = c;
    r_.drawSprite(sp::kStar[r - sp::kStarMinR], iround(x) - r, iround(y) - r, pg::Color::Black);
  }

 private:
  // Pixels whose centres fall in [xa, xb]; never empty, so thin rings stay closed.
  void span(float xa, float xb, int y, uint16_t c) {
    int a = static_cast<int>(std::ceil(xa - 0.5f)), b = static_cast<int>(std::floor(xb - 0.5f));
    if (b < a) a = b = static_cast<int>(std::floor((xa + xb) * 0.5f));
    rect(a, y, b - a + 1, 1, c);
  }

  pg::Renderer& r_;
};

void draw_scenery(Painter& p, float time) {
  constexpr float kClouds[2][3] = {{40, 22, 4}, {210, 38, 2.6f}};
  for (const auto& c : kClouds) {
    const float x = std::fmod(c[0] + time * c[2], 400.0f) - 40, y = c[1];
    spr(1, mix(backdrop(y), kWhite, 0.85f));
    spr(2, mix(backdrop(y), kWhite, 0.45f));
    p.art(sp::kCloud, x, y);
  }
  const uint16_t shimmer = rgb565(mix(backdrop(67), kWhite, 0.55f));
  float px = 0, py = 67 + std::sin(time * 1.2f) * 1.2f;
  for (int x = 6; x <= 320; x += 6) {
    const float y = 67 + std::sin(static_cast<float>(x) / 14.0f + time * 1.2f) * 1.2f;
    p.line(px, py, static_cast<float>(x), y, 1.5f, shimmer);
    px = static_cast<float>(x);
    py = y;
  }
}

void draw_bubble(Painter& p, const Bubble& b) {
  const float s = f(b.s * (b.sx + b.sy) * 0.5);
  const float a = clampf(f(b.alpha), 0, 1);
  if (s <= 0 || a <= 0.02f) return;
  const float x = f(b.x), y = f(b.y), rad = R * s;
  const Swatch c = swatch(b.color);
  if (b.glow > 0) p.ring(x, y, (R + 5 + 2 * f(b.glow)) * s, 6 * s, kHintGlow, a * (0.35f + 0.5f * f(b.glow)));
  int ri = iround(rad);
  if (ri < sp::kBubbleMinR) {
    p.annulus(x, y, rad, -1, c.base, a);
    return;
  }
  if (ri > sp::kBubbleMaxR) ri = sp::kBubbleMaxR;
  const uint32_t bd = backdrop(y);
  auto put = [&](int i, uint32_t rgb) { spr(i, mix(bd, rgb, a)); };
  for (int i = 1; i <= 9; ++i) {
    const float t = static_cast<float>(i - 1) / 8.0f;
    put(i, t < 0.55f ? mix(c.light, c.base, t / 0.55f) : mix(c.base, c.shadow, (t - 0.55f) / 0.45f));
  }
  put(10, mix(c.base, kWhite, 0.45f));
  put(11, c.shadow);
  put(12, mix(bd, c.shadow, 0.5f));
  put(13, mix(backdrop(y + rad + 6), kInk, 0.15f));
  put(14, mix(mix(c.light, c.base, 0.3f), kWhite, 0.75f));
  put(15, mix(c.base, kWhite, 0.9f));
  p.art(sp::kBubble[ri - sp::kBubbleMinR], x, y);
  p.art(sp::kGlyph[static_cast<int>(pattern_for(b.color))][ri - sp::kBubbleMinR], x, y);
}

void draw_crab(Painter& p, const ColorPopGame& game) {
  const Crab& c = game.crab();
  const float time = f(game.time());
  const float clap = f(c.clap);
  const float jump = clap > 0 ? -std::sin((1 - clap / 0.6f) * kPi) * 8 : 0;
  const float dance = c.dance > 0 ? std::sin(f(c.dance) * 10) : 0;
  const float ox = 42 + dance * 2, oy = 222 + f(c.y_off) + jump - std::fabs(dance) * 6;
  const uint16_t dark = rgb565(kCrabDark), body = rgb565(kCrabBody), ink = rgb565(kInk);
  for (int s : {-1, 1}) {  // legs
    for (int i = 0; i < 3; ++i) {
      const float fi = static_cast<float>(i);
      p.line(ox + static_cast<float>(s) * 14, oy - 6 + fi * 4, ox + static_cast<float>(s) * (24 + fi * 2),
             oy + 2 + fi * 3 + std::sin(time * 6 + fi), 3, dark);
    }
  }
  const float clap_t = clap > 0 ? std::fabs(std::sin((1 - clap / 0.6f) * kPi * 3)) : 0;
  for (int si : {-1, 1}) {  // arms + claws
    const float s = static_cast<float>(si);
    float cx = s * (30 - clap_t * 14), cy = -24 - clap_t * 6;
    if (c.pointing && si == 1) {
      const float dx = f(c.point_x) - 42, dy = f(c.point_y) - 222;
      float d = std::hypot(dx, dy);
      if (d == 0) d = 1;
      cx = 18 + dx / d * 18;
      cy = -16 + dy / d * 18;
    }
    p.quad(ox + s * 14, oy - 12, ox + s * 22, oy - 14, ox + cx, oy + cy, 3, dark);
    const float ang = std::atan2(cy + 12, cx - s * 14);
    p.disc(ox + cx, oy + cy, 10, dark);
    p.disc(ox + cx, oy + cy, 8, body);
    p.line(ox + cx, oy + cy, ox + cx + std::cos(ang) * 9, oy + cy + std::sin(ang) * 9, 2, dark);  // pincer
  }
  spr(1, kCrabBody);
  spr(2, kCrabDark);
  spr(3, mix(kCrabBody, kCheek, 0.45f));
  spr(4, mix(backdrop(oy - 10), kCrabDark, 0.5f));
  p.art(sp::kCrabBody, ox, oy);
  float ln = std::hypot(f(c.look_x), f(c.look_y));
  if (ln == 0) ln = 1;
  const float lx = f(c.look_x) / ln * 1.8f, ly = f(c.look_y) / ln * 1.8f;
  for (int si : {-1, 1}) {  // eye stalks
    const float s = static_cast<float>(si);
    p.line(ox + s * 6, oy - 20, ox + s * 8, oy - 31, 2.5f, dark);
    if (c.blink < 0) {
      p.line(ox + s * 8 - 4, oy - 32, ox + s * 8 + 4, oy - 32, 2.5f, dark);
      continue;
    }
    p.disc(ox + s * 8, oy - 33, 5.5f, dark);
    p.disc(ox + s * 8, oy - 33, 4.5f, rgb565(kWhite));
    p.disc(ox + s * 8 + lx, oy - 33 + ly, 2.2f, ink);
  }
  const bool happy = c.clap > 0 || c.dance > 0 || c.giggle > 0;
  p.arc(ox, oy - 10, happy ? 6.0f : 4.0f, 0.15f * kPi, 0.85f * kPi, 2, ink);
}

void draw_helper_hand(Painter& p, const ColorPopGame& game) {
  if (game.phase() != Phase::PLAY || game.hint() < 2) return;
  for (std::size_t i = 0; i < game.bubble_count(); ++i) {
    const Bubble& b = game.bubble(i);
    if (b.color != game.target() || b.state != BubbleState::FLOAT) continue;
    const float y = f(b.y) + 30 - std::fabs(std::sin(f(game.time()) * 4)) * 6;
    spr(1, kWhite);
    spr(2, kInk);
    spr(3, mix(backdrop(y), kInk, 0.5f));
    p.art(sp::kHand, f(b.x) + 14, y);
    return;
  }
}

void draw_fx(Painter& p, const ColorPopGame& game) {
  for (std::size_t i = 0; i < game.decor_count(); ++i) {
    const Decor& d = game.decor(i);
    p.ring(f(d.x), f(d.y), f(d.r), 1.2f, kWhite, 0.7f);
  }
  for (std::size_t i = 0; i < game.ring_count(); ++i) {
    const RingFx& r = game.ring(i);
    const float k = clampf(f(r.t / r.dur), 0, 1);
    p.ring(f(r.x), f(r.y), lerpf(f(r.r0), f(r.r1), ease_out_cubic(k)), f(r.w), r.color, 1 - k);
  }
  for (std::size_t i = 0; i < game.particle_count(); ++i) {
    const Particle& q = game.particle(i);
    const float a = clampf(f(q.life / q.max * 1.6), 0, 1);
    if (a <= 0.02f) continue;
    const float x = f(q.x), y = f(q.y), size = f(q.size);
    const uint16_t c = rgb565(mix(backdrop(y), q.color, a));
    if (q.shape == ParticleShape::STAR) {
      const int r = iround(size);
      if (r < sp::kStarMinR) {
        p.dot(x, y, 2, c);
      } else {
        p.star(r > sp::kStarMaxR ? sp::kStarMaxR : r, x, y, c);
      }
    } else if (q.shape == ParticleShape::RECT) {
      const float cs = std::fabs(std::cos(f(q.rot))), sn = std::fabs(std::sin(f(q.rot)));
      const int w = iround(size * cs + size * 0.5f * sn), h = iround(size * 0.5f * cs + size * sn);
      p.rect(iround(x - static_cast<float>(w) * 0.5f), iround(y - static_cast<float>(h) * 0.5f), w < 1 ? 1 : w,
             h < 1 ? 1 : h, c);
    } else {
      p.disc(x, y, size * (0.5f + 0.5f * f(q.life / q.max)), c);
    }
  }
}

void draw_badge(Painter& p, const ColorPopGame& game) {
  if (!game.has_target() || game.phase() == Phase::INTRO) return;
  const float time = f(game.time());
  const Swatch c = swatch(game.target());
  const float cx = 160, cy = 32 + std::sin(time * 2.2f) * 1.5f;
  if (game.hint() >= 1 && game.phase() == Phase::PLAY) {
    p.ring(cx, cy, 29, 4, kHintGlow, 0.4f + 0.4f * std::sin(time * 2 * kPi / kGlowPeriod));
  }
  p.annulus(cx, cy, 27, 25, kBorder, 1);
  p.annulus(cx, cy, 25, -1, kCream, 1);
  p.annulus(cx, cy, 20, -1, c.base, 1);
  if (game.badge().happy > 0) {
    const uint16_t white = rgb565(kWhite);
    p.disc(cx - 7, cy - 4, 2.5f, white);
    p.disc(cx + 7, cy - 4, 2.5f, white);
    p.arc(cx, cy + 2, 8, 0.15f * kPi, 0.85f * kPi, 2.5f, white);
  } else {
    spr(15, mix(c.base, kWhite, 0.95f));
    spr(10, mix(c.base, kWhite, 0.5f));
    p.art(sp::kGlyph[static_cast<int>(pattern_for(game.target()))][25 - sp::kBubbleMinR], cx, cy);
  }
}

void draw_chrome(Painter& p, bool mic_enabled) {
  const uint32_t sky = backdrop(28);
  spr(1, kCream);
  spr(2, kBorder);
  spr(3, kInk);
  spr(4, mix(sky, kBorder, 0.5f));
  spr(5, mix(kCream, kInk, 0.5f));
  spr(6, mix(sky, kCream, 0.5f));
  p.art(sp::kHomeKey, 0, 0);
  if (mic_enabled) {
    p.art(sp::kMicKeyOn, 0, 0);
  } else {  // faded (0.45) while voice is off
    const uint32_t cream = mix(sky, kCream, 0.45f);
    spr(7, cream);
    spr(8, mix(cream, kInk, 0.45f));
    spr(9, mix(cream, sky, 0.5f));
    p.art(sp::kMicKeyOff, 0, 0);
  }
}
}  // namespace

void ColorPopScene::draw_background(pg::Renderer& r) {
  Painter p(r);
  for (int y = 0; y < 70; ++y) {
    p.rect(0, y, FrameSurface::W, 1, rgb565(mix(kSkyTop, kSkyBot, (static_cast<float>(y) + 0.5f) / 70.0f)));
  }
  for (int y = 64; y < 209; ++y) {
    p.rect(0, y, FrameSurface::W, 1, rgb565(mix(kSeaTop, kSeaBot, (static_cast<float>(y) + 0.5f - 64) / 141.0f)));
  }
  spr(1, kSand);
  spr(2, mix(backdrop(201), kSand, 0.5f));
  spr(3, mix(kSand, kSandShadow, 0.3f));
  spr(4, mix(kSand, kSandShadow, 0.45f));
  spr(5, mix(kSand, kSandShadow, 0.6f));
  p.art(sp::kSandLeft, 0, 0);
  p.art(sp::kSandRight, 0, 0);
}

void ColorPopScene::draw(pg::Renderer& renderer) {
  if (!game_ || !surface_ || !surface_->ready()) return;
  pg::setCustomPalette(g_prim);
  pg::setSpriteCustomPaletteSlot(kSpriteSlot, g_spr);
  if (!background_ready_) {
    draw_background(renderer);
    surface_->save_background();
    background_ready_ = true;
  }
  surface_->begin_frame();
  Painter p(renderer);
  const ColorPopGame& game = *game_;
  draw_scenery(p, f(game.time()));
  for (std::size_t i = 0; i < game.bubble_count(); ++i) {
    const Bubble& b = game.bubble(i);
    if (b.t >= 0 && b.state != BubbleState::GONE) draw_bubble(p, b);
  }
  draw_crab(p, game);
  draw_helper_hand(p, game);
  draw_fx(p, game);
  draw_badge(p, game);
  draw_chrome(p, game.voice_enabled());
}

void ColorPopScene::processTouchEvents(pixelroot32::input::TouchEvent* events, uint8_t count) {
  if (!stage_) return;
  for (uint8_t i = 0; i < count; ++i) {
    if (events[i].getType() == pixelroot32::input::TouchEventType::TouchDown) {
      stage_->deliver_touch(events[i].x, events[i].y);
    }
  }
}

}  // namespace little_arcade::color_pop::pr32
