#include "little_arcade/games/color_pop_paint.hpp"

#include <cmath>
#include <initializer_list>

namespace little_arcade::color_pop {

namespace {
using ui::c2d::Cap;
using ui::c2d::Context;
using ui::c2d::Join;
using ui::c2d::Op;
using ui::c2d::rgb;

constexpr double kPi = 3.14159265358979323846;
constexpr double W = 320, H = 240, R = ColorPopGame::R;
constexpr uint32_t kInk = 0x3B3550, kCream = 0xFFF3D6, kHintGlow = 0xFFE066;
constexpr uint32_t kSkyTop = 0xBFE6FF, kSeaTop = 0xA9DBFB, kSeaBot = 0x6FBDF2, kSand = 0xF3DDB0,
                   kSandShadow = 0xDDB892;
constexpr double kBreathScale = 0.025, kGlowPeriod = 0.9;

double clamp(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
double ease_out_cubic(double t) {
  const double u = 1 - t;
  return 1 - u * u * u;
}
// Single-precision trig: the ESP32-S3 has no double FPU, so std::sin(double)
// is emulated in software and dominated frame time.
inline double fsin(double a) { return std::sin(static_cast<float>(a)); }
inline double fcos(double a) { return std::cos(static_cast<float>(a)); }

void star_path(Context& ctx, double cx, double cy, double ro, double ri, double rot) {
  ctx.begin_path();
  for (int i = 0; i < 10; ++i) {
    const double r = i % 2 ? ri : ro, a = rot + i * kPi / 5;
    ctx.line_to(cx + fcos(a) * r, cy + fsin(a) * r);
  }
  ctx.close_path();
}

void glyph(Context& ctx, Pattern kind, double s, ui::c2d::Color color) {
  ctx.set_fill(color);
  ctx.set_stroke(color);
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_line_join(Join::ROUND);
  if (kind == Pattern::HEART) {
    ctx.begin_path();
    ctx.move_to(0, s * 0.55);
    ctx.bezier_curve_to(-s * 1.1, -s * 0.1, -s * 0.5, -s * 0.95, 0, -s * 0.35);
    ctx.bezier_curve_to(s * 0.5, -s * 0.95, s * 1.1, -s * 0.1, 0, s * 0.55);
    ctx.fill();
  } else if (kind == Pattern::STAR) {
    star_path(ctx, 0, 0, s, s * 0.45, -kPi / 2);
    ctx.fill();
  } else if (kind == Pattern::WAVE) {
    ctx.set_line_width(3);
    for (double oy : {-s * 0.3, s * 0.3}) {
      ctx.begin_path();
      for (int i = 0; i <= 20; ++i) {
        const double x = -s + i * s / 10;
        ctx.line_to(x, oy + fsin(i / 20.0 * kPi * 4) * s * 0.22);
      }
      ctx.stroke();
    }
  } else if (kind == Pattern::LEAF) {
    ctx.begin_path();
    ctx.move_to(-s * 0.8, s * 0.6);
    ctx.quadratic_curve_to(-s * 0.8, -s * 0.8, s * 0.8, -s * 0.6);
    ctx.quadratic_curve_to(s * 0.8, s * 0.8, -s * 0.8, s * 0.6);
    ctx.fill();
  }
}

void round_rect(Context& ctx, double x, double y, double w, double h, double r) {
  ctx.begin_path();
  ctx.move_to(x + r, y);
  ctx.arc_to(x + w, y, x + w, y + h, r);
  ctx.arc_to(x + w, y + h, x, y + h, r);
  ctx.arc_to(x, y + h, x, y, r);
  ctx.arc_to(x, y, x + w, y, r);
  ctx.close_path();
}

void draw_background(Context& ctx, double time) {
  ctx.set_fill(rgb(0xFFFFFF, 0.85f));  // drifting clouds
  constexpr double clouds[2][3] = {{40, 22, 4}, {210, 38, 2.6}};
  for (const auto& c : clouds) {
    const double x = std::fmod(c[0] + time * c[2], W + 80) - 40, y = c[1];
    ctx.begin_path();
    ctx.arc(x, y, 9, 0, 7);
    ctx.arc(x + 11, y - 4, 11, 0, 7);
    ctx.arc(x + 23, y, 8, 0, 7);
    ctx.fill();
  }
  ctx.set_stroke(rgb(0xFFFFFF, 0.55f));  // horizon shimmer
  ctx.set_line_width(1.5);
  ctx.begin_path();
  for (int x = 0; x <= W; x += 6) ctx.line_to(x, 67 + fsin(x / 14.0 + time * 1.2) * 1.2);
  ctx.stroke();
}

void draw_bubble(Context& ctx, const Bubble& b, double time) {
  const Swatch c = swatch(b.color);
  ctx.save();
  ctx.set_global_alpha(b.alpha);
  ctx.translate(b.x, b.y);
  ctx.rotate(b.rot);
  ctx.scale(b.sx * b.s, b.sy * b.s);
  if (b.glow > 0) {
    ctx.set_stroke(rgb(kHintGlow));
    ctx.set_global_alpha(b.alpha * (0.35 + 0.5 * b.glow));
    ctx.set_line_width(6);
    ctx.begin_path();
    ctx.arc(0, 0, R + 5 + 2 * b.glow, 0, 7);
    ctx.stroke();
    ctx.set_global_alpha(b.alpha);
  }
  ctx.set_fill(rgb(0x3B3550, 0.15f));
  ctx.begin_path();
  ctx.ellipse(2, R + 6, R * 0.6, 4, 0, 0, 7);
  ctx.fill();
  const int g = ctx.create_radial_gradient(-10, -12, 4, 0, 0, R);
  ctx.add_color_stop(g, 0, rgb(c.light));
  ctx.add_color_stop(g, 0.55, rgb(c.base));
  ctx.add_color_stop(g, 1, rgb(c.shadow));
  ctx.set_fill_gradient(g);
  ctx.begin_path();
  ctx.arc(0, 0, R, 0, 7);
  ctx.fill();
  ctx.set_stroke(rgb(c.shadow));
  ctx.set_line_width(2);
  ctx.stroke();
  ctx.set_stroke(ui::c2d::hsla(std::fmod(time * 60 + b.ph * 40, 360.0), 90, 80, 0.35f));  // iridescent rim
  ctx.set_line_width(2.5);
  ctx.begin_path();
  ctx.arc(0, 0, R - 3, -0.4 + b.ph * 0.3, 1.4 + b.ph * 0.3);
  ctx.stroke();
  glyph(ctx, pattern_for(b.color), 12, rgb(0xFFFFFF, 0.9f));
  ctx.set_fill(rgb(0xFFFFFF, 0.75f));
  ctx.begin_path();
  ctx.ellipse(-11, -14, 7, 4, -0.6, 0, 7);
  ctx.fill();
  ctx.begin_path();
  ctx.arc(-3, -20, 1.8, 0, 7);
  ctx.fill();
  ctx.restore();
}

void draw_crab(Context& ctx, const ColorPopGame& game) {
  const Crab& c = game.crab();
  const double time = game.time();
  constexpr uint32_t body = 0xFF8A2A, dark = 0xC2621B;
  ctx.save();
  ctx.translate(42, 222 + c.y_off);
  const double jump = c.clap > 0 ? -fsin((1 - c.clap / 0.6) * kPi) * 8 : 0;
  const double dance = c.dance > 0 ? fsin(c.dance * 10) : 0;
  ctx.translate(0, jump - std::fabs(dance) * 6);
  ctx.rotate((c.giggle > 0 ? fsin(c.giggle * 40) * 0.12 : 0) + dance * 0.15);
  const double br = fsin(time * 2 * kPi / 2.8) * kBreathScale;
  ctx.scale(1 + br, 1 - br);
  ctx.set_stroke(rgb(dark));
  ctx.set_line_width(3);
  ctx.set_line_cap(Cap::ROUND);
  for (int s : {-1, 1}) {
    for (int i = 0; i < 3; ++i) {  // legs
      ctx.begin_path();
      ctx.move_to(s * 14, -6 + i * 4);
      ctx.line_to(s * (24 + i * 2), 2 + i * 3 + fsin(time * 6 + i) * 1);
      ctx.stroke();
    }
  }
  const double clap_t = c.clap > 0 ? std::fabs(fsin((1 - c.clap / 0.6) * kPi * 3)) : 0;
  for (int s : {-1, 1}) {  // arms + claws
    double cx = s * (30 - clap_t * 14), cy = -24 - clap_t * 6;
    if (c.pointing && s == 1) {
      const double dx = c.point_x - 42, dy = c.point_y - 222;
      double d = std::hypot(dx, dy);
      if (d == 0) d = 1;
      cx = 18 + dx / d * 18;
      cy = -16 + dy / d * 18;
    }
    ctx.begin_path();
    ctx.move_to(s * 14, -12);
    ctx.quadratic_curve_to(s * 22, -14, cx, cy);
    ctx.stroke();
    const double ang = std::atan2(cy + 12, cx - s * 14);
    ctx.set_fill(rgb(body));
    ctx.begin_path();
    ctx.move_to(cx, cy);
    ctx.arc(cx, cy, 9, ang + 0.45, ang + kPi * 2 - 0.45);
    ctx.close_path();
    ctx.fill();
    ctx.set_stroke(rgb(dark));
    ctx.set_line_width(2);
    ctx.stroke();
    ctx.set_line_width(3);
  }
  ctx.set_fill(rgb(body));
  ctx.begin_path();
  ctx.ellipse(0, -10, 20, 13, 0, 0, 7);
  ctx.fill();
  ctx.set_stroke(rgb(dark));
  ctx.set_line_width(2);
  ctx.stroke();
  double ln = std::hypot(c.look_x, c.look_y);
  if (ln == 0) ln = 1;
  const double lx = c.look_x / ln * 1.8, ly = c.look_y / ln * 1.8;
  for (int s : {-1, 1}) {  // eye stalks
    ctx.set_stroke(rgb(dark));
    ctx.set_line_width(2.5);
    ctx.begin_path();
    ctx.move_to(s * 6, -20);
    ctx.line_to(s * 8, -31);
    ctx.stroke();
    if (c.blink < 0) {
      ctx.begin_path();
      ctx.move_to(s * 8 - 4, -32);
      ctx.line_to(s * 8 + 4, -32);
      ctx.stroke();
      continue;
    }
    ctx.set_fill(rgb(0xFFFFFF));
    ctx.begin_path();
    ctx.arc(s * 8, -33, 5, 0, 7);
    ctx.fill();
    ctx.set_stroke(rgb(dark));
    ctx.set_line_width(1.5);
    ctx.stroke();
    ctx.set_fill(rgb(kInk));
    ctx.begin_path();
    ctx.arc(s * 8 + lx, -33 + ly, 2.2, 0, 7);
    ctx.fill();
  }
  ctx.set_stroke(rgb(kInk));  // mouth
  ctx.set_line_width(2);
  ctx.begin_path();
  const bool happy = c.clap > 0 || c.dance > 0 || c.giggle > 0;
  ctx.arc(0, -10, happy ? 6 : 4, 0.15 * kPi, 0.85 * kPi);
  ctx.stroke();
  ctx.set_fill(rgb(0xF06BB8, 0.45f));
  ctx.begin_path();
  ctx.arc(-12, -8, 3, 0, 7);
  ctx.arc(12, -8, 3, 0, 7);
  ctx.fill();
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
  ctx.set_global_alpha(mic_enabled ? 1.0 : 0.45);  // Mic (faded when voice is off)
  ctx.set_fill(rgb(kCream));
  round_rect(ctx, 270, 6, 44, 44, 14);
  ctx.fill();
  ctx.set_fill(rgb(kInk));
  round_rect(ctx, 287, 14, 10, 18, 5);
  ctx.fill();
  ctx.set_stroke(rgb(kInk));
  ctx.set_line_width(2);
  ctx.begin_path();
  ctx.arc(292, 25, 9, 0.1 * kPi, 0.9 * kPi);
  ctx.stroke();
  ctx.begin_path();
  ctx.move_to(292, 34);
  ctx.line_to(292, 40);
  ctx.stroke();
  ctx.set_global_alpha(1);
}

void draw_badge(Context& ctx, const ColorPopGame& game) {
  if (!game.has_target() || game.phase() == Phase::INTRO) return;
  const double time = game.time();
  const Swatch c = swatch(game.target());
  const double bob = fsin(time * 2.2) * 1.5;
  const double press = 1 + clamp(-game.badge().sv * 0.012, -0.1, 0.12);
  ctx.save();
  ctx.translate(160, 32 + bob);
  ctx.scale(press, 2 - press);
  if (game.hint() >= 1 && game.phase() == Phase::PLAY) {
    ctx.set_stroke(rgb(kHintGlow));
    ctx.set_line_width(4);
    ctx.set_global_alpha(0.4 + 0.4 * fsin(time * 2 * kPi / kGlowPeriod));
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
  ctx.set_fill(rgb(c.base));
  ctx.begin_path();
  ctx.arc(0, 0, 20, 0, 7);
  ctx.fill();
  if (game.badge().happy > 0) {
    ctx.set_fill(rgb(0xFFFFFF));
    ctx.begin_path();
    ctx.arc(-7, -4, 2.5, 0, 7);
    ctx.arc(7, -4, 2.5, 0, 7);
    ctx.fill();
    ctx.set_stroke(rgb(0xFFFFFF));
    ctx.set_line_width(2.5);
    ctx.begin_path();
    ctx.arc(0, 2, 8, 0.15 * kPi, 0.85 * kPi);
    ctx.stroke();
  } else {
    glyph(ctx, pattern_for(game.target()), 10, rgb(0xFFFFFF, 0.95f));
  }
  ctx.restore();
}

void draw_helper_hand(Context& ctx, const ColorPopGame& game) {
  if (game.phase() != Phase::PLAY || game.hint() < 2) return;
  const Bubble* b = nullptr;
  for (std::size_t i = 0; i < game.bubble_count(); ++i) {
    const Bubble& o = game.bubble(i);
    if (o.color == game.target() && o.state == BubbleState::FLOAT) {
      b = &o;
      break;
    }
  }
  if (!b) return;
  const double y = b->y + 30 + std::fabs(fsin(game.time() * 4)) * -6;
  ctx.save();
  ctx.translate(b->x + 14, y);
  ctx.rotate(-0.5);
  ctx.set_fill(rgb(0xFFFFFF));
  ctx.set_stroke(rgb(kInk));
  ctx.set_line_width(1.5);
  round_rect(ctx, -4, -16, 8, 18, 4);
  ctx.fill();
  ctx.stroke();
  ctx.begin_path();
  ctx.ellipse(0, 6, 9, 8, 0, 0, 7);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

void draw_fx(Context& ctx, const ColorPopGame& game) {
  for (std::size_t i = 0; i < game.decor_count(); ++i) {
    const Decor& d = game.decor(i);
    ctx.set_stroke(rgb(0xFFFFFF, 0.7f));
    ctx.set_line_width(1.2);
    ctx.begin_path();
    ctx.arc(d.x, d.y, d.r, 0, 7);
    ctx.stroke();
  }
  for (std::size_t i = 0; i < game.ring_count(); ++i) {
    const RingFx& r = game.ring(i);
    const double k = r.t / r.dur;
    ctx.set_global_alpha(1 - k);
    ctx.set_stroke(rgb(r.color));
    ctx.set_line_width(r.w);
    ctx.begin_path();
    ctx.arc(r.x, r.y, lerp(r.r0, r.r1, ease_out_cubic(k)), 0, 7);
    ctx.stroke();
  }
  for (std::size_t i = 0; i < game.particle_count(); ++i) {
    const Particle& p = game.particle(i);
    ctx.set_global_alpha(clamp(p.life / p.max * 1.6, 0, 1));
    ctx.set_composite(p.add ? Op::LIGHTER : Op::SOURCE_OVER);
    ctx.set_fill(rgb(p.color));
    if (p.shape == ParticleShape::STAR) {
      star_path(ctx, p.x, p.y, p.size, p.size * 0.45, p.rot);
      ctx.fill();
    } else if (p.shape == ParticleShape::RECT) {
      ctx.save();
      ctx.translate(p.x, p.y);
      ctx.rotate(p.rot);
      ctx.fill_rect(-p.size / 2, -p.size / 4, p.size, p.size / 2);
      ctx.restore();
    } else {
      ctx.begin_path();
      ctx.arc(p.x, p.y, p.size * (0.5 + 0.5 * p.life / p.max), 0, 7);
      ctx.fill();
    }
  }
  ctx.set_global_alpha(1);
  ctx.set_composite(Op::SOURCE_OVER);
}

}  // namespace

void paint_background(Context& ctx) {
  int g = ctx.create_linear_gradient(0, 0, 0, 70);
  ctx.add_color_stop(g, 0, rgb(kSkyTop));
  ctx.add_color_stop(g, 1, rgb(0xE6F5FF));
  ctx.set_fill_gradient(g);
  ctx.fill_rect(0, 0, W, 70);
  g = ctx.create_linear_gradient(0, 64, 0, 205);
  ctx.add_color_stop(g, 0, rgb(kSeaTop));
  ctx.add_color_stop(g, 1, rgb(kSeaBot));
  ctx.set_fill_gradient(g);
  ctx.fill_rect(0, 64, W, 145);
  ctx.set_fill(rgb(kSand));
  ctx.begin_path();
  ctx.move_to(0, 204);
  for (int x = 0; x <= W; x += 8) ctx.line_to(x, 202 + fsin(x / 23.0) * 3);
  ctx.line_to(W, H);
  ctx.line_to(0, H);
  ctx.fill();
  Mulberry32 r(7);
  ctx.set_fill(rgb(kSandShadow));
  for (int i = 0; i < 70; ++i) {
    ctx.set_global_alpha(0.35 + r.next() * 0.3);
    ctx.begin_path();
    const double x = r.next() * W;
    const double y = 210 + r.next() * 30;
    const double rad = 0.6 + r.next() * 1.2;
    ctx.arc(x, y, rad, 0, 7);
    ctx.fill();
  }
  ctx.set_global_alpha(1);
}

void paint_frame(Context& ctx, const ColorPopGame& game, bool mic_enabled) {
  draw_background(ctx, game.time());
  for (std::size_t i = 0; i < game.bubble_count(); ++i) {
    const Bubble& b = game.bubble(i);
    if (b.t >= 0) draw_bubble(ctx, b, game.time());
  }
  draw_crab(ctx, game);
  draw_helper_hand(ctx, game);
  draw_fx(ctx, game);
  draw_badge(ctx, game);
  draw_chrome(ctx, mic_enabled);
}

}  // namespace little_arcade::color_pop
