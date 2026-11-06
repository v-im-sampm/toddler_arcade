#include "little_arcade/games/color_pop.hpp"

#include <cmath>

namespace little_arcade::color_pop {

namespace {
constexpr double kPi = 3.14159265358979323846;
// Reference TOK values.
constexpr double kSquashPressX = 0.88, kSquashPressY = 1.12;
constexpr double kWiggleDeg = 6, kWiggleS = 0.36, kGlowPeriod = 0.9;
constexpr double kIdleVisualHint = 6, kIdleSpokenHint = 12, kMaxInputBlock = 0.3;
constexpr int kAutoHelpAfter = 2;
constexpr double kSpringK = 380, kSpringC = 14, kBreathScale = 0.025, kBob = 2, kTapDebounce = 0.08;

constexpr Color kNames[kColorCount] = {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW};

double clamp(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
double ease_out_cubic(double t) { return 1 - std::pow(1 - t, 3); }
double ease_out_back(double t) {
  const double c1 = 1.70158, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
}
bool in_rect(double x, double y, const Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

constexpr uint32_t kSparkleColors[] = {0xFFEB9C, 0xFFFBEF};
constexpr uint32_t kConfettiColors[] = {0xEF4B4F, 0xFFD23F, 0x2DB26E, 0x3A6FE8, 0xF06BB8};
}  // namespace

void ColorPopGame::enter(GameContext& context) {
  ctx_ = &context;
  // Reference: grng = mulberry32(seed), frng = Math.random. Seeded per entry
  // so every visit differs but a seeded run is replayable.
  grng_.seed(context.rng.next());
  frng_.seed(context.rng.next());
  phase_ = Phase::INTRO;
  time_ = phase_t_ = last_dt_ = 0;
  round_ = successes_ = 0;
  count_ = 2;
  has_target_ = has_last_ = false;
  bag_n_ = 0;
  misses_ = hint_ = 0;
  idle_ = 0;
  spoken_ = round_auto_help_ = false;
  auto_streak_ = 0;
  celebrate_for_ = 0;
  bubble_n_ = respawn_n_ = part_n_ = ring_n_ = decor_n_ = timer_n_ = 0;
  crab_ = Crab{};
  badge_ = Badge{};
  play(Cue::AMBIENT, 0);  // reference startWaves()
}

void ColorPopGame::exit() {
  ctx_ = nullptr;
  bubble_n_ = respawn_n_ = part_n_ = ring_n_ = decor_n_ = timer_n_ = 0;
}

uint32_t ColorPopGame::active_contexts() const { return static_cast<uint32_t>(Context::Colors); }

void ColorPopGame::play(Cue c, uint8_t variant) {
  if (ctx_) ctx_->feedback.play(c, variant);
}

void ColorPopGame::schedule(double delay, Cue c, uint8_t variant) {
  if (timer_n_ >= 4) return;
  timers_[timer_n_++] = Timer{time_ + delay, c, variant};
}

void ColorPopGame::emit(double x, double y, int n, const EmitOpts& o) {
  if (n < 1) n = 1;
  for (int i = 0; i < n; ++i) {
    if (part_n_ >= kMaxParticles) {  // evict oldest, never exceed the cap
      for (std::size_t k = 1; k < part_n_; ++k) parts_[k - 1] = parts_[k];
      --part_n_;
    }
    const double a = frng_.next() * kPi * 2;
    const double sp = lerp(o.speed0, o.speed1, frng_.next());
    const double life = lerp(o.life0, o.life1, frng_.next());
    Particle p{};
    p.x = x;
    p.y = y;
    p.vx = std::cos(a) * sp;
    p.vy = std::sin(a) * sp - o.up;
    p.g = o.gravity;
    p.life = life;
    p.max = life;
    p.shape = o.shape;
    p.color = o.colors[static_cast<int>(frng_.next() * o.color_count)];
    p.size = o.size * lerp(0.7, 1.3, frng_.next());
    p.rot = frng_.next() * 6.28;
    p.vr = (frng_.next() - 0.5) * 8;
    p.add = o.additive;
    parts_[part_n_++] = p;
  }
}

namespace {
constexpr double kSparkle[] = {40, 110, 0.35, 0.7, 60};
}

void ColorPopGame::add_ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w) {
  if (ring_n_ >= kMaxRings) {
    for (std::size_t k = 1; k < ring_n_; ++k) rings_[k - 1] = rings_[k];
    --ring_n_;
  }
  rings_[ring_n_++] = RingFx{x, y, r0, r1, 0, dur, color, w};
}

void ColorPopGame::spawn_decor(double x, double y, double r, double vy) {
  if (decor_n_ >= kMaxDecor) {
    for (std::size_t k = 1; k < decor_n_; ++k) decor_[k - 1] = decor_[k];
    --decor_n_;
  }
  decor_[decor_n_++] = Decor{x, y, r, vy, frng_.next() * 6.28};
}

Bubble ColorPopGame::make_bubble(Color c, uint8_t slot, double delay) {
  const double sx = kSlots[slot][0], sy = kSlots[slot][1];
  const double jx = grng_.next() * 12 - 6;
  const double jy = grng_.next() * 12 - 6;
  Bubble b{};
  b.color = c;
  b.slot = slot;
  b.bx = sx + jx;
  b.by = sy + jy;
  b.x = sx + jx;
  b.y = 250;
  b.state = BubbleState::RISE;
  b.t = -delay;
  b.s = 0.4;
  b.sv = 0;
  b.sx = 1;
  b.sy = 1;
  b.rot = 0;
  b.wig = 0;
  b.glow = 0;
  b.alpha = 1;
  b.ph = frng_.next() * 6.28;
  b.last_tap = -9;
  return b;
}

Color ColorPopGame::next_target() {
  if (bag_n_ == 0) {
    for (std::size_t i = 0; i < kColorCount; ++i) bag_[i] = kNames[i];
    bag_n_ = static_cast<int>(kColorCount);
    for (int i = bag_n_ - 1; i > 0; --i) {
      const int j = static_cast<int>(grng_.next() * (i + 1));
      const Color t = bag_[i];
      bag_[i] = bag_[j];
      bag_[j] = t;
    }
    if (has_last_ && bag_[bag_n_ - 1] == last_) {
      const Color t = bag_[0];
      bag_[0] = bag_[bag_n_ - 1];
      bag_[bag_n_ - 1] = t;
    }
  }
  return bag_[--bag_n_];
}

void ColorPopGame::new_round() {
  ++round_;
  misses_ = 0;
  hint_ = 0;
  idle_ = 0;
  spoken_ = false;
  round_auto_help_ = false;
  target_ = next_target();
  has_target_ = true;
  last_ = target_;
  has_last_ = true;
  Color others[kColorCount - 1];
  int on = 0;
  for (Color n : kNames) {
    if (n != target_) others[on++] = n;
  }
  for (int i = on - 1; i > 0; --i) {
    const int j = static_cast<int>(grng_.next() * (i + 1));
    const Color t = others[i];
    others[i] = others[j];
    others[j] = t;
  }
  Color colors[kColorCount];
  colors[0] = target_;
  int cn = 1;
  for (int i = 0; i < count_ - 1 && i < on; ++i) colors[cn++] = others[i];
  uint8_t slots[kSlotCount] = {0, 1, 2, 3, 4, 5};
  for (int i = 5; i > 0; --i) {
    const int j = static_cast<int>(grng_.next() * (i + 1));
    const uint8_t t = slots[i];
    slots[i] = slots[j];
    slots[j] = t;
  }
  bubble_n_ = 0;
  for (int i = 0; i < cn; ++i) bubbles_[bubble_n_++] = make_bubble(colors[i], slots[i], i * 0.12);
  respawn_n_ = 0;
  crab_.pointing = false;
  phase_ = Phase::PLAY;
  phase_t_ = 0;
  badge_.sv = -6;
  schedule(0.25, Cue::PROMPT, 0);  // prompt is non-blocking
}

void ColorPopGame::set_hint(int level) {
  if (level > hint_) {
    for (std::size_t i = 0; i < bubble_n_; ++i) {
      Bubble& b = bubbles_[i];
      if (b.color == target_ && b.state == BubbleState::FLOAT) {
        b.wig = kWiggleS;
        break;
      }
    }
  }
  hint_ = level;
  if (level >= 2) round_auto_help_ = true;
}

void ColorPopGame::pop(Bubble& b) {
  const Swatch c = swatch(b.color);
  const bool is_t = b.color == target_;
  b.state = BubbleState::GONE;
  const uint32_t burst[2] = {c.base, c.light};
  emit(b.x, b.y, is_t ? 14 : 7,
       EmitOpts{50, 130, 0.3, 0.6, 120, ParticleShape::CIRCLE, is_t ? 5.0 : 4.0, 0, burst, 2, false});
  add_ring(b.x, b.y, R * 0.8, R * 1.8, 0.45, c.light, is_t ? 3 : 2);
  if (is_t) {
    emit(b.x, b.y, 16,
         EmitOpts{kSparkle[0], kSparkle[1], kSparkle[2], kSparkle[3], kSparkle[4], ParticleShape::STAR, 5, 0,
                  kSparkleColors, 2, true});
    play(Cue::SUCCESS, static_cast<uint8_t>(b.color));  // plop + marimba(degree)
    crab_.clap = 0.6;
    badge_.happy = 1.2;
    badge_.sv = -10;
    ++successes_;
    const bool big = successes_ % 5 == 0;
    if (successes_ % 3 == 0 && count_ < 4) ++count_;
    if (round_auto_help_) {
      if (++auto_streak_ >= 2 && count_ > 2) {
        --count_;
        auto_streak_ = 0;
      }
    } else {
      auto_streak_ = 0;
    }
    for (std::size_t i = 0; i < bubble_n_; ++i) {
      Bubble& o = bubbles_[i];
      if (o.state == BubbleState::FLOAT || o.state == BubbleState::RISE) {
        o.state = BubbleState::LEAVE;
        o.t = 0;
      }
    }
    respawn_n_ = 0;
    phase_ = Phase::CELEBRATE;
    phase_t_ = 0;
    celebrate_for_ = big ? 2.0 : 1.5;
    schedule(0.18, Cue::CELEBRATE, big ? 1 : 0);
    if (big) {
      crab_.dance = 2.0;
      emit(160, 120, 36,
           EmitOpts{60, 160, 0.9, 1.5, 140, ParticleShape::RECT, 6, 60, kConfettiColors, 5, false});
      for (int i = 0; i < 20; ++i) {
        const double y = 250 + frng_.next() * 40;
        const double r = 4 + frng_.next() * 6;
        const double vy = -90 - frng_.next() * 60;
        spawn_decor(10 + i * 16, y, r, vy);
      }
    }
  } else {
    const int deg = static_cast<int>(grng_.next() * 5);
    play(Cue::NOTE, static_cast<uint8_t>(cue::kOtherPop | deg));  // plop + marimba(deg, oct 1)
    ++misses_;
    set_hint(misses_ >= kAutoHelpAfter ? 2 : 1);
    if (respawn_n_ < kMaxRespawns) respawns_[respawn_n_++] = Respawn{b.color, b.slot, time_ + 1.0};
  }
}

void ColorPopGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  double dt = dt_ms / 1000.0;
  if (dt > 0.05) dt = 0.05;
  step(dt);
}

void ColorPopGame::step(double dt) {
  last_dt_ = dt;
  time_ += dt;
  phase_t_ += dt;
  // setTimeout() callbacks that are due.
  for (std::size_t i = 0; i < timer_n_;) {
    if (time_ >= timers_[i].at) {
      const Timer t = timers_[i];
      for (std::size_t k = i + 1; k < timer_n_; ++k) timers_[k - 1] = timers_[k];
      --timer_n_;
      play(t.cue, t.cue == Cue::PROMPT ? static_cast<uint8_t>(target_) : t.variant);
    } else {
      ++i;
    }
  }

  if (phase_ == Phase::INTRO) {
    crab_.y_off = lerp(40, 0, ease_out_back(clamp(phase_t_ / 0.6, 0, 1)));
    if (phase_t_ >= 1.2) {
      crab_.y_off = 0;
      new_round();
    }
  } else if (phase_ == Phase::PLAY) {
    idle_ += dt;
    if (idle_ > kIdleVisualHint && hint_ < 1) set_hint(1);
    if (idle_ > kIdleSpokenHint && !spoken_) {
      spoken_ = true;
      play(Cue::PROMPT, static_cast<uint8_t>(target_));
      badge_.sv = -8;
    }
  } else if (phase_ == Phase::CELEBRATE && phase_t_ >= celebrate_for_) {
    new_round();
  }

  {
    Respawn snapshot[kMaxRespawns];
    const std::size_t n = respawn_n_;
    for (std::size_t i = 0; i < n; ++i) snapshot[i] = respawns_[i];
    for (std::size_t i = 0; i < n; ++i) {
      const Respawn r = snapshot[i];
      if (!(time_ >= r.at && phase_ == Phase::PLAY)) continue;
      for (std::size_t k = 0; k < respawn_n_; ++k) {
        if (respawns_[k].color == r.color && respawns_[k].slot == r.slot && respawns_[k].at == r.at) {
          for (std::size_t m = k + 1; m < respawn_n_; ++m) respawns_[m - 1] = respawns_[m];
          --respawn_n_;
          break;
        }
      }
      if (bubble_n_ < kMaxBubbles) bubbles_[bubble_n_++] = make_bubble(r.color, r.slot, 0);
    }
  }

  const Bubble* tb = nullptr;
  for (std::size_t i = 0; i < bubble_n_; ++i) {
    const Bubble& b = bubbles_[i];
    if (b.color == target_ && (b.state == BubbleState::FLOAT || b.state == BubbleState::RISE)) {
      tb = &b;
      break;
    }
  }
  crab_.pointing = hint_ >= 1 && tb != nullptr;
  if (crab_.pointing) {
    crab_.point_x = tb->x;
    crab_.point_y = tb->y;
    crab_.look_x = tb->x - 42;
    crab_.look_y = tb->y - 190;
  }

  for (std::size_t i = 0; i < bubble_n_; ++i) {
    Bubble& b = bubbles_[i];
    b.t += dt;
    b.ph += dt;
    const bool is_t = b.color == target_;
    double s_target = 1;
    if (phase_ == Phase::PLAY && is_t && hint_ >= 2) s_target = 1.15 + 0.04 * std::sin(time_ * 7);
    if (b.state == BubbleState::RISE) {
      if (b.t < 0) {
        b.y = 260;
        continue;
      }
      const double k = clamp(b.t / 0.6, 0, 1);
      b.y = lerp(250, b.by, ease_out_cubic(k));
      if (k >= 1) b.state = BubbleState::FLOAT;
    } else if (b.state == BubbleState::FLOAT) {
      const double m = 1;
      b.y = b.by + std::sin(b.ph * 2.1) * kBob * m;
      b.x = b.bx + std::sin(b.ph * 1.3) * 1.5 * m;
    } else if (b.state == BubbleState::PRESS) {
      b.sx = kSquashPressX;
      b.sy = kSquashPressY * 1.02;  // pre-pop stretch
      if (b.t >= 0.08) pop(b);
    } else if (b.state == BubbleState::LEAVE) {
      b.y -= dt * (60 + b.t * 120);
      b.x += std::sin(b.ph * 3) * 0.4;
      b.alpha = clamp(1 - b.t / 0.9, 0, 1);
      if (b.alpha <= 0) b.state = BubbleState::GONE;
    }
    for (int k = 0; k < 2; ++k) {  // spring substeps
      const double h = dt / 2, a = -kSpringK * (b.s - s_target) - kSpringC * b.sv;
      b.sv += a * h;
      b.s += b.sv * h;
    }
    if (b.state != BubbleState::PRESS) {
      const double br = std::sin(b.ph * 2 * kPi / 2.8) * kBreathScale;
      b.sx = lerp(b.sx, 1 + br, 0.25);
      b.sy = lerp(b.sy, 1 - br, 0.25);
    }
    b.wig = std::fmax(0, b.wig - dt);
    b.rot = b.wig > 0 ? std::sin((1 - b.wig / kWiggleS) * kPi * 6) * kWiggleDeg * kPi / 180 : 0;
    b.glow = (phase_ == Phase::PLAY && is_t && hint_ >= 1) ? 0.5 + 0.5 * std::sin(time_ * 2 * kPi / kGlowPeriod) : 0;
  }
  {
    std::size_t n = 0;
    for (std::size_t i = 0; i < bubble_n_; ++i) {
      if (bubbles_[i].state != BubbleState::GONE) bubbles_[n++] = bubbles_[i];
    }
    bubble_n_ = n;
  }

  badge_.happy = std::fmax(0, badge_.happy - dt);

  crab_.blink -= dt;
  if (crab_.blink < -0.12) crab_.blink = 2 + frng_.next() * 4;
  crab_.clap = std::fmax(0, crab_.clap - dt);
  crab_.giggle = std::fmax(0, crab_.giggle - dt);
  crab_.dance = std::fmax(0, crab_.dance - dt);

  {
    std::size_t n = 0;
    for (std::size_t i = 0; i < part_n_; ++i) {
      Particle& p = parts_[i];
      p.life -= dt;
      p.vy += p.g * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.rot += p.vr * dt;
    }
    for (std::size_t i = 0; i < part_n_; ++i) {
      if (parts_[i].life > 0) parts_[n++] = parts_[i];
    }
    part_n_ = n;
  }
  {
    std::size_t n = 0;
    for (std::size_t i = 0; i < ring_n_; ++i) rings_[i].t += dt;
    for (std::size_t i = 0; i < ring_n_; ++i) {
      if (rings_[i].t < rings_[i].dur) rings_[n++] = rings_[i];
    }
    ring_n_ = n;
  }
  {
    int small = 0;
    for (std::size_t i = 0; i < decor_n_; ++i) small += decor_[i].r < 3.5 ? 1 : 0;
    if (small < 4 && frng_.next() < dt * 0.8) {
      const double x = 90 + frng_.next() * 220;
      const double r = 1.5 + frng_.next() * 1.5;
      const double vy = -18 - frng_.next() * 10;
      spawn_decor(x, 205, r, vy);
    }
    std::size_t n = 0;
    for (std::size_t i = 0; i < decor_n_; ++i) {
      Decor& d = decor_[i];
      d.ph += dt;
      d.y += d.vy * dt;
      d.x += std::sin(d.ph * 3) * 0.2;
    }
    for (std::size_t i = 0; i < decor_n_; ++i) {
      if (decor_[i].y > -20) decor_[n++] = decor_[i];
    }
    decor_n_ = n;
  }
}

void ColorPopGame::frame_drawn() {
  // drawBadge(): `badge.sv *= 0.86` once per drawn 60 Hz frame (skipped while
  // the badge is hidden). Time-based so slower device frames decay alike.
  if (!has_target_ || phase_ == Phase::INTRO) return;
  badge_.sv *= std::pow(0.86, last_dt_ * 60.0);
}

void ColorPopGame::replay_prompt() {
  if (time_ - badge_.last_tap < 1) set_hint(hint_ > 1 ? hint_ : 1);
  badge_.last_tap = time_;
  badge_.sv = -8;
  play(Cue::PROMPT, static_cast<uint8_t>(target_));
  idle_ = 0;
}

void ColorPopGame::pointer_down(double x, double y) {
  crab_.look_x = x - 42;
  crab_.look_y = y - 190;
  // Home and Mic keys are consumed by the SceneManager before they reach us.
  if (phase_ == Phase::INTRO) {
    phase_t_ = 99;  // intro skippable
    return;
  }
  if (phase_ == Phase::CELEBRATE) {
    if (phase_t_ > kMaxInputBlock) {
      emit(x, y, 6,
           EmitOpts{kSparkle[0], kSparkle[1], kSparkle[2], kSparkle[3], kSparkle[4], ParticleShape::STAR, 5, 0,
                    kSparkleColors, 2, true});
      const int deg = static_cast<int>(frng_.next() * 5);
      play(Cue::NOTE, static_cast<uint8_t>(cue::kSparkleTap | deg));
    }
    return;
  }
  if (in_rect(x, y, kTargetBadge)) {  // badge: replay; double tap = help
    replay_prompt();
    return;
  }
  Bubble* best = nullptr;
  double bd = kHit;
  for (std::size_t i = 0; i < bubble_n_; ++i) {
    Bubble& b = bubbles_[i];
    if (b.state != BubbleState::FLOAT && b.state != BubbleState::RISE) continue;
    const double d = std::hypot(b.x - x, b.y - y);
    if (d < bd) {
      bd = d;
      best = &b;
    }
  }
  if (best) {
    if (time_ - best->last_tap < kTapDebounce) return;
    best->last_tap = time_;
    best->state = BubbleState::PRESS;
    best->t = 0;
    idle_ = 0;
    return;
  }
  if (in_rect(x, y, kCrab)) {
    crab_.giggle = 0.5;
    play(Cue::REACTION, cue::kCrabGiggle);
    return;
  }
  add_ring(x, y, 3, 22, 0.5, 0xFFFFFF, 2);
  spawn_decor(x, y, 3, -30);
  play(Cue::NOTE, cue::kRipple);
  idle_ = 0;
}

void ColorPopGame::press_color(Color c) {
  for (std::size_t i = 0; i < bubble_n_; ++i) {
    Bubble& b = bubbles_[i];
    if (b.color != c || (b.state != BubbleState::FLOAT && b.state != BubbleState::RISE)) continue;
    if (time_ - b.last_tap < kTapDebounce) return;
    b.last_tap = time_;
    b.state = BubbleState::PRESS;
    b.t = 0;
    idle_ = 0;
    return;
  }
  // Named a colour that is not floating: show the answer, gently.
  set_hint(hint_ > 1 ? hint_ : 1);
  play(Cue::PROMPT, static_cast<uint8_t>(target_));
}

void ColorPopGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  if (event.source == Source::TOUCH) {
    pointer_down(event.x, event.y);
    return;
  }
  // Voice (not in the touch-only reference): same outcomes as the touch path.
  if (phase_ == Phase::INTRO) {
    phase_t_ = 99;
    return;
  }
  if (phase_ != Phase::PLAY) return;
  switch (event.intent) {
    case Intent::RED: press_color(Color::RED); break;
    case Intent::BLUE: press_color(Color::BLUE); break;
    case Intent::GREEN: press_color(Color::GREEN); break;
    case Intent::YELLOW: press_color(Color::YELLOW); break;
    case Intent::AGAIN: replay_prompt(); break;
    case Intent::HELP:
      set_hint(hint_ > 1 ? hint_ : 1);
      replay_prompt();
      break;
    default: break;
  }
}

}  // namespace little_arcade::color_pop
