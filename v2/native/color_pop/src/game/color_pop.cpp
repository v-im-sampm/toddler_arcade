#include "game/color_pop.hpp"

#include <algorithm>
#include <cmath>

namespace color_pop {
namespace {
constexpr double kPi = 3.141592653589793;
constexpr int kPent[5] = {0, 2, 4, 7, 9};
constexpr double kWiggle = 0.36, kWiggleDeg = 6, kGlowPeriod = 0.9;
constexpr double kIdleVisual = 6, kIdleSpoken = 12, kMaxInputBlock = 0.3, kTapDebounce = 0.08;
constexpr double kSpringK = 380, kSpringC = 14, kBreath = 0.025, kBob = 2;
constexpr uint32_t kWhite = 0xFFFFFF;
constexpr uint32_t kSparkleColors[2] = {0xFFEB9C, 0xFFFBEF};
constexpr uint32_t kConfettiColors[5] = {0xEF4B4F, 0xFFD23F, 0x2DB26E, 0x3A6FE8, 0xF06BB8};

double clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
double ease_out_cubic(double t) { return 1 - std::pow(1 - t, 3); }
double ease_out_back(double t) {
  const double c1 = 1.70158, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
}
bool in_rect(double x, double y, double rx, double ry, double rw, double rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
int js_round(double v) { return static_cast<int>(std::floor(v + 0.5)); }
const Palette& pal(Color c) { return kPalette[static_cast<int>(c)]; }
}  // namespace

double degree_hz(int degree, int octave) {
  const int o = static_cast<int>(std::floor(degree / 5.0));
  const int i = ((degree % 5) + 5) % 5;
  return 523.25 * std::pow(2.0, (kPent[i] + 12.0 * (o + octave)) / 12.0);
}

Game::Game(Sound& sound, uint32_t seed) : sound_(sound), grng_(seed), frng_(seed ^ 0x9E3779B9u) {}

Color Game::next_target() {
  if (bag_.empty()) {
    bag_.assign(std::begin(kNames), std::end(kNames));
    for (int i = static_cast<int>(bag_.size()) - 1; i > 0; --i) {
      const int j = static_cast<int>(grng_.next() * (i + 1));
      std::swap(bag_[i], bag_[j]);
    }
    if (has_last_ && bag_.back() == last_) std::swap(bag_.front(), bag_.back());
  }
  const Color c = bag_.back();
  bag_.pop_back();
  return c;
}

Bubble Game::make_bubble(Color color, int slot, double delay) {
  const double sx = kSlots[slot][0], sy = kSlots[slot][1];
  const double jx = grng_.next() * 12 - 6, jy = grng_.next() * 12 - 6;
  Bubble b{};
  b.color = color; b.slot = slot;
  b.bx = sx + jx; b.by = sy + jy; b.x = sx + jx; b.y = 250;
  b.state = BubbleState::Rise; b.t = -delay;
  b.s = 0.4; b.sv = 0; b.sx = 1; b.sy = 1; b.rot = 0; b.wig = 0; b.glow = 0; b.alpha = 1;
  b.ph = frng_.next() * 6.28; b.last_tap = -9;
  return b;
}

void Game::new_round() {
  ++round_; misses_ = 0; hint_ = 0; idle_ = 0; spoken_ = false; round_auto_help_ = false;
  target_ = next_target(); has_target_ = true;
  last_ = target_; has_last_ = true;
  std::vector<Color> others;
  for (Color n : kNames) if (n != target_) others.push_back(n);
  for (int i = static_cast<int>(others.size()) - 1; i > 0; --i) {
    const int j = static_cast<int>(grng_.next() * (i + 1));
    std::swap(others[i], others[j]);
  }
  std::vector<Color> colors{target_};
  for (int i = 0; i < count_ - 1 && i < static_cast<int>(others.size()); ++i) colors.push_back(others[i]);
  int slots[6] = {0, 1, 2, 3, 4, 5};
  for (int i = 5; i > 0; --i) {
    const int j = static_cast<int>(grng_.next() * (i + 1));
    std::swap(slots[i], slots[j]);
  }
  bubbles_.clear();
  for (std::size_t i = 0; i < colors.size(); ++i) bubbles_.push_back(make_bubble(colors[i], slots[i], i * 0.12));
  respawns_.clear();
  crab_.pointing = false;
  phase_ = Phase::Play; t_ = 0;
  badge_.sv = -6;
  sound_.motif(target_, 0.25);
}

void Game::start_game() {
  if (!started_audio_) { started_audio_ = true; sound_.start(); }
  phase_ = Phase::Intro; t_ = 0; crab_.y_off = 40;
}

void Game::home() {
  bubbles_.clear(); parts_.clear(); rings_.clear();
  phase_ = Phase::Start; t_ = 0; crab_.y_off = 40;
}

void Game::set_hint(int level) {
  if (level > hint_) {
    for (Bubble& b : bubbles_) {
      if (b.color == target_ && b.state == BubbleState::Float) { b.wig = kWiggle; break; }
    }
  }
  hint_ = level;
  if (level >= 2) round_auto_help_ = true;
}

void Game::ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w) {
  if (static_cast<int>(rings_.size()) >= kMaxRings) rings_.erase(rings_.begin());
  rings_.push_back(Ring{x, y, r0, r1, 0, dur, color, w});
}

void Game::spawn_decor(double x, double y, double r, double vy) {
  const double ph = frng_.next() * 6.28;
  if (static_cast<int>(decor_.size()) >= kMaxDecor) decor_.erase(decor_.begin());
  decor_.push_back(Decor{x, y, r, vy, ph});
}

void Game::emit(double x, double y, int n, const EmitSpec& o) {
  n = std::max(1, js_round(n * (reduced_ ? 0.5 : 1)));
  for (int i = 0; i < n; ++i) {
    if (static_cast<int>(parts_.size()) >= kMaxParticles) parts_.erase(parts_.begin());
    const double a = frng_.next() * kPi * 2;
    const double sp = lerp(o.sp0, o.sp1, frng_.next());
    const double life = lerp(o.l0, o.l1, frng_.next());
    Particle p{};
    p.x = x; p.y = y; p.vx = std::cos(a) * sp; p.vy = std::sin(a) * sp - o.up; p.g = o.gravity;
    p.life = life; p.max = life; p.shape = o.shape;
    p.color = o.colors[static_cast<int>(frng_.next() * o.ncolors)];
    p.size = o.size * lerp(0.7, 1.3, frng_.next());
    p.rot = frng_.next() * 6.28;
    p.vr = (frng_.next() - 0.5) * 8;
    p.additive = o.additive;
    parts_.push_back(p);
  }
}

void Game::badge_tap() {
  if (time_ - badge_.last_tap < 1) set_hint(std::max(hint_, 1));
  badge_.last_tap = time_; badge_.sv = -8;
  sound_.motif(target_, 0); idle_ = 0;
}

void Game::pop(Bubble& b) {
  const Palette& c = pal(b.color);
  const bool is_target = b.color == target_;
  b.state = BubbleState::Gone;
  const uint32_t colors[2] = {c.base, c.light};
  emit(b.x, b.y, is_target ? 14 : 7,
       EmitSpec{50, 130, 0.3, 0.6, 120, Shape::Circle, is_target ? 5.0 : 4.0, colors, 2, 0, false});
  ring(b.x, b.y, kRadius * 0.8, kRadius * 1.8, 0.45, c.light, is_target ? 3 : 2);
  if (is_target) {
    emit(b.x, b.y, 16, EmitSpec{40, 110, 0.35, 0.7, 60, Shape::Star, 5, kSparkleColors, 2, 0, true});
    sound_.plop(0, 1, 1);
    sound_.marimba(degree_hz(c.degree), 0.03, 1, 0.55);
    crab_.clap = 0.6; badge_.happy = 1.2; badge_.sv = -10;
    ++successes_;
    const bool big = successes_ % 5 == 0;
    if (successes_ % 3 == 0 && count_ < 4) ++count_;
    if (round_auto_help_) {
      if (++auto_streak_ >= 2 && count_ > 2) { --count_; auto_streak_ = 0; }
    } else {
      auto_streak_ = 0;
    }
    for (Bubble& o : bubbles_) {
      if (o.state == BubbleState::Float || o.state == BubbleState::Rise) { o.state = BubbleState::Leave; o.t = 0; }
    }
    respawns_.clear();
    phase_ = Phase::Celebrate; t_ = 0; celebrate_for_ = big ? 2.0 : 1.5;
    sound_.celebrate(big, 0.18);
    if (big) {
      crab_.dance = 2.0;
      emit(160, 120, 36, EmitSpec{60, 160, 0.9, 1.5, 140, Shape::Rect, 6, kConfettiColors, 5, 60, false});
      if (!reduced_) {
        for (int i = 0; i < 20; ++i) {
          const double y = 250 + frng_.next() * 40;
          const double r = 4 + frng_.next() * 6;
          const double vy = -90 - frng_.next() * 60;
          spawn_decor(10 + i * 16, y, r, vy);
        }
      }
    }
  } else {
    sound_.plop(0, 0.55, 1.4);
    sound_.marimba(degree_hz(static_cast<int>(grng_.next() * 5), 1), 0.02, 0.45, 0.35);
    ++misses_;
    set_hint(misses_ >= 2 ? 2 : 1);
    respawns_.push_back(Respawn{b.color, b.slot, time_ + 1.0});
  }
}

void Game::down(double x, double y) {
  crab_.look_x = x - 42; crab_.look_y = y - 190;
  if (phase_ == Phase::Start) { start_game(); return; }
  if (in_rect(x, y, 0, 0, 56, 56)) { home(); return; }
  if (in_rect(x, y, 264, 0, 56, 56)) { ring(292, 28, 6, 24, 0.4, kWhite, 2); return; }
  if (phase_ == Phase::Intro) { t_ = 99; return; }
  if (phase_ == Phase::Celebrate) {
    if (t_ > kMaxInputBlock) {
      emit(x, y, 6, EmitSpec{40, 110, 0.35, 0.7, 60, Shape::Star, 5, kSparkleColors, 2, 0, true});
      sound_.marimba(degree_hz(static_cast<int>(frng_.next() * 5), 1), 0, 0.4, 0.3);
    }
    return;
  }
  if (in_rect(x, y, 132, 4, 56, 56)) { badge_tap(); return; }
  Bubble* best = nullptr;
  double bd = kHit;
  for (Bubble& b : bubbles_) {
    if (b.state != BubbleState::Float && b.state != BubbleState::Rise) continue;
    const double d = std::hypot(b.x - x, b.y - y);
    if (d < bd) { bd = d; best = &b; }
  }
  if (best) {
    if (time_ - best->last_tap < kTapDebounce) return;
    best->last_tap = time_; best->state = BubbleState::Press; best->t = 0; idle_ = 0;
    return;
  }
  if (in_rect(x, y, 4, 186, 76, 54)) { crab_.giggle = 0.5; sound_.plop(0, 0.6, 1.6); return; }
  ring(x, y, 3, 22, 0.5, kWhite, 2);
  spawn_decor(x, y, 3, -30);
  sound_.marimba(degree_hz(0, 1), 0, 0.3, 0.25);
  idle_ = 0;
}

void Game::intent(little_arcade::Intent intent) {
  using little_arcade::Intent;
  Color wanted;
  switch (intent) {
    case Intent::RED: wanted = Color::Red; break;
    case Intent::BLUE: wanted = Color::Blue; break;
    case Intent::GREEN: wanted = Color::Green; break;
    case Intent::YELLOW: wanted = Color::Yellow; break;
    case Intent::START: case Intent::GO:
      if (phase_ == Phase::Start) down(160, 120);
      else if (phase_ == Phase::Intro) t_ = 99;
      return;
    case Intent::BACK: home(); return;
    case Intent::AGAIN:
      if (phase_ == Phase::Play) badge_tap();
      return;
    case Intent::HELP:
      if (phase_ == Phase::Play) {
        set_hint(std::min(2, hint_ + 1));
        sound_.motif(target_, 0);
        idle_ = 0;
      }
      return;
    default: return;
  }
  if (phase_ == Phase::Start || phase_ == Phase::Intro) { down(160, 120); return; }
  if (phase_ != Phase::Play) return;
  for (const Bubble& b : bubbles_) {
    if (b.color == wanted && (b.state == BubbleState::Float || b.state == BubbleState::Rise)) {
      down(b.x, b.y);  // exactly the touch path: same debounce, press and pop
      return;
    }
  }
  // Colour not on screen: replay the prompt kindly instead of doing nothing.
  sound_.motif(target_, 0);
  badge_.sv = -8;
  idle_ = 0;
}

int Game::helper_bubble() const {
  if (phase_ != Phase::Play || hint_ < 2) return -1;
  for (std::size_t i = 0; i < bubbles_.size(); ++i) {
    if (bubbles_[i].color == target_ && bubbles_[i].state == BubbleState::Float) return static_cast<int>(i);
  }
  return -1;
}

void Game::tick(double dt) {
  time_ += dt; t_ += dt;
  if (phase_ == Phase::Intro) {
    crab_.y_off = lerp(40, 0, ease_out_back(clamp(t_ / 0.6, 0, 1)));
    if (t_ >= 1.2) { crab_.y_off = 0; new_round(); }
  } else if (phase_ == Phase::Play) {
    idle_ += dt;
    if (idle_ > kIdleVisual && hint_ < 1) set_hint(1);
    if (idle_ > kIdleSpoken && !spoken_) { spoken_ = true; sound_.motif(target_, 0); badge_.sv = -8; }
  } else if (phase_ == Phase::Celebrate && t_ >= celebrate_for_) {
    new_round();
  }

  for (std::size_t i = 0; i < respawns_.size();) {
    if (time_ >= respawns_[i].at && phase_ == Phase::Play) {
      const Respawn r = respawns_[i];
      respawns_.erase(respawns_.begin() + static_cast<long>(i));
      bubbles_.push_back(make_bubble(r.color, r.slot, 0));
    } else {
      ++i;
    }
  }
  const Bubble* tb = nullptr;
  for (const Bubble& b : bubbles_) {
    if (b.color == target_ && (b.state == BubbleState::Float || b.state == BubbleState::Rise)) { tb = &b; break; }
  }
  crab_.pointing = hint_ >= 1 && tb;
  if (crab_.pointing) {
    crab_.point_x = tb->x; crab_.point_y = tb->y;
    crab_.look_x = tb->x - 42; crab_.look_y = tb->y - 190;
  }

  for (std::size_t i = 0; i < bubbles_.size(); ++i) {
    Bubble& b = bubbles_[i];
    b.t += dt; b.ph += dt;
    const bool is_target = b.color == target_;
    double s_target = 1;
    if (phase_ == Phase::Play && is_target && hint_ >= 2) s_target = 1.15 + 0.04 * std::sin(time_ * 7);
    if (b.state == BubbleState::Rise) {
      if (b.t < 0) { b.y = 260; continue; }
      const double k = clamp(b.t / 0.6, 0, 1);
      b.y = lerp(250, b.by, ease_out_cubic(k));
      if (k >= 1) b.state = BubbleState::Float;
    } else if (b.state == BubbleState::Float) {
      const double m = reduced_ ? 0.3 : 1;
      b.y = b.by + std::sin(b.ph * 2.1) * kBob * m;
      b.x = b.bx + std::sin(b.ph * 1.3) * 1.5 * m;
    } else if (b.state == BubbleState::Press) {
      b.sx = 0.88; b.sy = 1.12 * 1.02;
      if (b.t >= 0.08) pop(b);  // may push particles but never resizes bubbles_
    } else if (b.state == BubbleState::Leave) {
      b.y -= dt * (60 + b.t * 120);
      b.x += std::sin(b.ph * 3) * 0.4;
      b.alpha = clamp(1 - b.t / 0.9, 0, 1);
      if (b.alpha <= 0) b.state = BubbleState::Gone;
    }
    for (int k = 0; k < 2; ++k) {
      const double h = dt / 2, a = -kSpringK * (b.s - s_target) - kSpringC * b.sv;
      b.sv += a * h; b.s += b.sv * h;
    }
    if (b.state != BubbleState::Press) {
      const double br = std::sin(b.ph * 2 * kPi / 2.8) * kBreath * (reduced_ ? 0.4 : 1);
      b.sx = lerp(b.sx, 1 + br, 0.25); b.sy = lerp(b.sy, 1 - br, 0.25);
    }
    b.wig = std::max(0.0, b.wig - dt);
    b.rot = b.wig > 0 ? std::sin((1 - b.wig / kWiggle) * kPi * 6) * kWiggleDeg * kPi / 180 : 0;
    b.glow = (phase_ == Phase::Play && is_target && hint_ >= 1) ? 0.5 + 0.5 * std::sin(time_ * 2 * kPi / kGlowPeriod) : 0;
  }
  bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(),
                                [](const Bubble& b) { return b.state == BubbleState::Gone; }),
                 bubbles_.end());

  badge_.happy = std::max(0.0, badge_.happy - dt);
  crab_.blink -= dt;
  if (crab_.blink < -0.12) crab_.blink = 2 + frng_.next() * 4;
  crab_.clap = std::max(0.0, crab_.clap - dt);
  crab_.giggle = std::max(0.0, crab_.giggle - dt);
  crab_.dance = std::max(0.0, crab_.dance - dt);

  for (Particle& p : parts_) {
    p.life -= dt; p.vy += p.g * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.rot += p.vr * dt;
  }
  parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }), parts_.end());
  for (Ring& r : rings_) r.t += dt;
  rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const Ring& r) { return r.t >= r.dur; }), rings_.end());
  int small = 0;
  for (const Decor& d : decor_) small += d.r < 3.5 ? 1 : 0;
  if (small < 4 && frng_.next() < dt * 0.8) {
    const double x = 90 + frng_.next() * 220;
    const double r = 1.5 + frng_.next() * 1.5;
    const double vy = -18 - frng_.next() * 10;
    spawn_decor(x, 205, r, vy);
  }
  for (Decor& d : decor_) { d.ph += dt; d.y += d.vy * dt; d.x += std::sin(d.ph * 3) * 0.2; }
  decor_.erase(std::remove_if(decor_.begin(), decor_.end(), [](const Decor& d) { return d.y <= -20; }), decor_.end());

  // drawBadge(): sv *= 0.86 per rendered frame while the badge is visible.
  if (has_target_ && phase_ != Phase::Start && phase_ != Phase::Intro) badge_.sv *= std::pow(0.86, dt * 60);
}

}  // namespace color_pop
