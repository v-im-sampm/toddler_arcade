#include "little_arcade/games/hatch_a_pet.hpp"

#include <cmath>

namespace little_arcade::hatch_a_pet {

namespace {
constexpr float kPi = 3.14159265f;
// Reference TOK values.
constexpr float kSpringK = 380, kSpringC = 14;
constexpr double kTapDebounce = 0.08;
constexpr double kIdleVisualHint = 6, kIdleSpokenHint = 12, kMaxInputBlock = 0.3;
constexpr double kChoiceWait = 5;  // TOK.choiceWait

// Positions and easing are single precision (no double FPU on the ESP32-S3).
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
// Phase clock k = t / dur in [0, 1]; the clock itself stays double.
float phase_k(double t, double dur) { return clamp(static_cast<float>(t / dur), 0, 1); }
bool in_rect(float x, float y, float rx, float ry, float rw, float rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// spring(): 2 substeps (stable at 60 Hz).
template <typename T>
void spring(T& o, float target, float dt) {
  for (int i = 0; i < 2; ++i) {
    const float h = dt / 2, a = -kSpringK * (o.q - target) - kSpringC * o.qv;
    o.qv += a * h;
    o.q += o.qv * h;
  }
}

constexpr uint32_t kSparkleColors[] = {0xFFEB9C, 0xFFFBEF};
constexpr uint32_t kCrumbColors[] = {0xFFF6E2, 0xE3CBA0};
constexpr uint32_t kHeartColors[] = {0xF06BB8, 0xFF9A92};
constexpr uint32_t kYesColors[] = {0x2DB26E, 0x8EE0B4};
constexpr uint32_t kNoColors[] = {0xF06BB8, 0xFFB3DD};
constexpr ParticleShape kConfettiShape[kEggPatternCount] = {ParticleShape::CIRCLE, ParticleShape::RECT,
                                                            ParticleShape::RECT, ParticleShape::STAR};

constexpr float kEggCy = HatchAPetGame::kEggY + HatchAPetGame::kCrackY;
}  // namespace

void HatchAPetGame::reset_to_start() {
  audio_ = false;
  phase_ = Phase::START;
  time_ = t_ = last_dt_ = 0;
  bag_n_ = 0;
  last_ = -1;
  hatches_ = 0;
  idle_ = 0;
  hint_ = 0;
  spoken_ = false;
  egg_ = Egg{};
  pet_ = Pet{};
  halves_ = Halves{};
  choices_ = Choices{};
  badge_ = Badge{};
  shelf_n_ = part_n_ = ring_n_ = decor_n_ = 0;
}

void HatchAPetGame::enter(GameContext& context) {
  ctx_ = &context;
  // Reference: grng = mulberry32(seed), frng = Math.random. Seeded per entry
  // so every visit differs but a seeded run is replayable.
  grng_.seed(context.rng.next());
  frng_.seed(context.rng.next());
  reset_to_start();
  // The launcher tap is the reference's tap-to-start.
  start_game();
}

void HatchAPetGame::exit() {
  ctx_ = nullptr;
  shelf_n_ = part_n_ = ring_n_ = decor_n_ = 0;
}

uint32_t HatchAPetGame::active_contexts() const { return static_cast<uint32_t>(Context::General); }

void HatchAPetGame::play(Cue c, uint8_t variant) {
  if (ctx_) ctx_->feedback.play(c, variant);
}

void HatchAPetGame::emit(float x, float y, int n, const EmitOpts& o) {
  if (n < 1) n = 1;
  for (int i = 0; i < n; ++i) {
    if (part_n_ >= kMaxParticles) {  // evict oldest, never exceed the cap
      for (std::size_t k = 1; k < part_n_; ++k) parts_[k - 1] = parts_[k];
      --part_n_;
    }
    const float a = static_cast<float>(frng_.next()) * kPi * 2;
    const float sp = lerp(o.speed0, o.speed1, static_cast<float>(frng_.next()));
    const float life = lerp(o.life0, o.life1, static_cast<float>(frng_.next()));
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
    p.size = o.size * lerp(0.7f, 1.3f, static_cast<float>(frng_.next()));
    p.rot = static_cast<float>(frng_.next()) * 6.28f;
    p.vr = (static_cast<float>(frng_.next()) - 0.5f) * 8;
    p.add = o.additive;
    parts_[part_n_++] = p;
  }
}

void HatchAPetGame::add_ring(float x, float y, float r0, float r1, float dur, uint32_t color, float w) {
  if (ring_n_ >= kMaxRings) {
    for (std::size_t k = 1; k < ring_n_; ++k) rings_[k - 1] = rings_[k];
    --ring_n_;
  }
  rings_[ring_n_++] = RingFx{x, y, r0, r1, 0, dur, color, w};
}

void HatchAPetGame::spawn_fluff(float x, float y) {
  Decor d{};
  d.x = x;
  d.y = y;
  d.vx = 10 + static_cast<float>(frng_.next()) * 8;
  d.ph = static_cast<float>(frng_.next()) * 6.28f;
  d.r = 2 + static_cast<float>(frng_.next()) * 1.5f;
  if (decor_n_ < kMaxDecor) decor_[decor_n_++] = d;
}

Creature HatchAPetGame::next_creature() {
  if (bag_n_ == 0) {
    for (int i = 0; i < static_cast<int>(kCreatureCount); ++i) bag_[i] = static_cast<Creature>(i);
    bag_n_ = kCreatureCount;
    for (int i = bag_n_ - 1; i > 0; --i) {
      const int j = static_cast<int>(grng_.next() * (i + 1));
      const Creature t = bag_[i];
      bag_[i] = bag_[j];
      bag_[j] = t;
    }
    if (static_cast<int>(bag_[bag_n_ - 1]) == last_) {
      const Creature t = bag_[0];
      bag_[0] = bag_[bag_n_ - 1];
      bag_[bag_n_ - 1] = t;
    }
  }
  return bag_[--bag_n_];
}

void HatchAPetGame::new_egg() {
  egg_.creature = next_creature();
  last_ = static_cast<int>(egg_.creature);
  egg_.pattern = static_cast<EggPattern>(static_cast<int>(grng_.next() * 4));
  egg_.need = 3 + static_cast<int>(grng_.next() * 3);
  egg_.cracks = 0;
  egg_.q = egg_.qv = egg_.ang = 0;
  egg_.wob = 0;
  egg_.next_wob = 3 + frng_.next() * 2;
  idle_ = 0;
  hint_ = 0;
  spoken_ = false;
}

void HatchAPetGame::start_game() {
  audio_ = true;
  play(Cue::AMBIENT, 0);  // initAudio() + startMusic(): master ramp-in + lullaby
  phase_ = Phase::INTRO;
  t_ = 0;
  new_egg();
  egg_.y_off = -200;
}

void HatchAPetGame::start_egg_play() {
  phase_ = Phase::EGG;
  t_ = 0;
  badge_.sv = -6;
  play(Cue::NOTE, cue::kKnockKnockLater);  // setTimeout(knockKnock, 200)
}

void HatchAPetGame::home() {
  part_n_ = ring_n_ = shelf_n_ = 0;
  halves_.on = false;
  choices_.show = false;
  choices_.out = 0;
  phase_ = Phase::START;
  t_ = 0;
  bag_n_ = 0;
  last_ = -1;
  egg_.y_off = 0;
}

void HatchAPetGame::knock() {
  egg_.cracks++;
  egg_.qv = -3.2f;
  idle_ = 0;
  hint_ = 0;
  spoken_ = false;
  egg_.wob = 0;
  play(Cue::NOTE, static_cast<uint8_t>(cue::kKnock | (egg_.cracks & 0x0F)));
  emit(kEggX, kEggCy, 6, EmitOpts{40, 100, 0.4f, 0.7f, 300, ParticleShape::CIRCLE, 3, 40, kCrumbColors, 2, false});
  add_ring(kEggX, kEggCy, 10, 44, 0.4f, 0xFFFFFF, 2);
  if (egg_.cracks >= egg_.need) {
    phase_ = Phase::PRE_HATCH;
    t_ = 0;
    play(Cue::NOTE, cue::kTrill);
  }
}

void HatchAPetGame::hatch() {
  phase_ = Phase::HATCH;
  t_ = 0;
  hatches_++;
  const Swatch c = swatch(egg_.creature);
  halves_.on = true;
  halves_.t = 0;
  halves_.top_x = halves_.top_y = 0;
  halves_.top_vx = 70;
  halves_.top_vy = -260;
  halves_.top_a = 0;
  halves_.top_va = 5;
  halves_.bot_y = 0;
  const uint32_t confetti[] = {c.base, c.light, 0xFFD23F, 0xFFFBEF};
  emit(kEggX, kEggCy, 30,
       EmitOpts{60, 170, 0.9f, 1.5f, 140, kConfettiShape[static_cast<int>(egg_.pattern)], 6, 60, confetti, 4, false});
  emit(kEggX, kEggCy, 16, EmitOpts{40, 110, 0.35f, 0.7f, 60, ParticleShape::STAR, 5, 0, kSparkleColors, 2, true});
  add_ring(kEggX, kEggCy, 20, 80, 0.6f, c.light, 3);
  pet_.x = kPetX;
  pet_.y = kPetY;
  pet_.s = 0.2f;
  pet_.q = pet_.qv = 0;
  pet_.react = -1;
  pet_.rt = 0;
  pet_.reacts = 0;
  pet_.t = 0;
  badge_.happy = 1.2;
  badge_.sv = -10;
  play(Cue::SUCCESS, 0);
}

void HatchAPetGame::react() {
  pet_.react = pet_.next;
  pet_.next = (pet_.next + 1) % 3;
  pet_.rt = pet_.react == 2 ? 0.8 : 0.7;
  pet_.reacts++;
  pet_.t = 0;
  choices_.idle = 0;
  if (pet_.react == 1) pet_.qv = -3;  // crouch before the jump
  play(Cue::REACTION, static_cast<uint8_t>((pet_.react << 4) | static_cast<int>(egg_.creature)));
}

void HatchAPetGame::choose_yes() {
  emit(kYesX, kYesY, 10, EmitOpts{50, 130, 0.3f, 0.6f, 120, ParticleShape::CIRCLE, 4, 0, kYesColors, 2, false});
  add_ring(kYesX, kYesY, 22, 48, 0.45f, 0x8EE0B4, 3);
  choices_.show = false;
  choices_.out = 0.35;
  play(Cue::NOTE, cue::kChoicePlop);
  play(Cue::REACTION, static_cast<uint8_t>(cue::kBye | static_cast<int>(egg_.creature)));
  if (shelf_n_ >= kShelfSlots) {  // oldest friend leaves
    for (std::size_t k = 1; k < shelf_n_; ++k) shelf_[k - 1] = shelf_[k];
    --shelf_n_;
    for (std::size_t i = 0; i < shelf_n_; ++i) shelf_[i].tx = kShelf[i][0];
  }
  pet_.from_x = pet_.x;
  pet_.from_y = pet_.y;
  pet_.to_x = kShelf[shelf_n_][0];
  pet_.to_y = kShelf[shelf_n_][1];
  phase_ = Phase::BYE;
  t_ = 0;
}

void HatchAPetGame::choose_no() {
  emit(kNoX, kNoY, 10, EmitOpts{50, 130, 0.3f, 0.6f, 120, ParticleShape::CIRCLE, 4, 0, kNoColors, 2, false});
  emit(pet_.x, pet_.y - 20, 6, EmitOpts{30, 70, 0.7f, 1.1f, -40, ParticleShape::HEART, 6, 0, kHeartColors, 2, false});
  add_ring(kNoX, kNoY, 22, 48, 0.45f, 0xFFB3DD, 3);
  choices_.show = false;
  choices_.out = 0.35;
  play(Cue::NOTE, cue::kChoicePlop);
  pet_.reacts = 0;
  pet_.t = 0;
  pet_.react = 0;
  pet_.rt = 0.7;
  play(Cue::REACTION, static_cast<uint8_t>(cue::kGiggle | static_cast<int>(egg_.creature)));
}

void HatchAPetGame::tap_badge() {
  badge_.last_tap = time_;
  badge_.sv = -12;
  idle_ = 0;
  choices_.idle = 0;
  const Swatch c = swatch(egg_.creature);
  add_ring(160, 32, 24, 52, 0.45f, c.light, 3);
  if (phase_ == Phase::EGG) {  // egg answers with a wiggle + sparkles
    const uint32_t colors[] = {0xFFD23F, c.light};
    emit(160, 62, 8, EmitOpts{40, 110, 0.35f, 0.7f, 60, ParticleShape::STAR, 5, 0, colors, 2, false});
    egg_.wob = 0.5;
    egg_.look_x = 0;
    egg_.look_y = -60;
    play(Cue::NOTE, cue::kKnockKnock);
  } else {  // pet looks up and giggles; hearts float
    const uint32_t colors[] = {c.base, c.light};
    emit(160, 62, 7, EmitOpts{40, 90, 0.7f, 1.1f, 70, ParticleShape::HEART, 6, 0, colors, 2, false});
    pet_.look_x = 0;
    pet_.look_y = -120;
    if (pet_.react < 0) {
      pet_.react = 0;
      pet_.rt = 0.7;
    }
    play(Cue::REACTION, static_cast<uint8_t>(cue::kNameChirp | static_cast<int>(egg_.creature)));
  }
}

void HatchAPetGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  double dt = dt_ms / 1000.0;
  if (dt > 0.05) dt = 0.05;
  step(dt);
}

void HatchAPetGame::step(double dt) {
  last_dt_ = dt;
  time_ += dt;
  t_ += dt;
  if (phase_ == Phase::INTRO) {
    const float k = phase_k(t_, 0.7);
    egg_.y_off = lerp(-200, 0, ease_out_cubic(k));
    if (k >= 1 && !egg_.landed) {
      egg_.landed = true;
      egg_.qv = -4;
      play(Cue::NOTE, cue::kIntroPlop);
      emit(kNestX, kNestY, 8, EmitOpts{40, 100, 0.4f, 0.7f, 300, ParticleShape::CIRCLE, 3, 40, kCrumbColors, 2, false});
    }
    if (t_ >= 1.2) {
      egg_.y_off = 0;
      egg_.landed = false;
      start_egg_play();
    }
  } else if (phase_ == Phase::EGG) {
    idle_ += dt;
    if (idle_ > kIdleVisualHint && hint_ < 1) hint_ = 1;
    if (idle_ > kIdleSpokenHint && !spoken_) {
      spoken_ = true;
      hint_ = 2;
      play(Cue::NOTE, cue::kKnockKnock);
      badge_.sv = -8;
    }
    egg_.next_wob -= dt;
    if (egg_.next_wob <= 0) {
      egg_.wob = 0.5;
      egg_.next_wob = hint_ >= 1 ? 1.6 : 3 + frng_.next() * 2;
    }
  } else if (phase_ == Phase::PRE_HATCH) {
    if (t_ >= 0.4) hatch();
  } else if (phase_ == Phase::HATCH) {
    if (t_ >= 1.6) {
      phase_ = Phase::PET;
      t_ = 0;
      pet_.t = 0;
      play(Cue::REACTION, static_cast<uint8_t>(cue::kNameChirp | static_cast<int>(egg_.creature)));
      pet_.react = 1;
      pet_.rt = 0.7;
      pet_.qv = -3;
    }
  } else if (phase_ == Phase::PET) {
    pet_.t += dt;
    if (!choices_.show && choices_.out <= 0 && (pet_.reacts >= 3 || pet_.t >= kChoiceWait)) {
      choices_.show = true;
      choices_.t = 0;
      choices_.idle = 0;
      play(Cue::NOTE, cue::kChoice);
    }
    if (choices_.show) {
      choices_.t += dt;
      choices_.idle += dt;
    }
  } else if (phase_ == Phase::BYE) {
    const float k = phase_k(t_, 0.9);
    pet_.x = lerp(pet_.from_x, pet_.to_x, k);
    pet_.y = lerp(pet_.from_y, pet_.to_y, k) - std::sin(k * kPi) * 40;
    pet_.s = lerp(1, kMini, ease_out_cubic(k));
    if (k >= 1) {
      Friend f{};
      f.c = egg_.creature;
      f.x = pet_.to_x;
      f.y = pet_.to_y;
      f.tx = pet_.to_x;
      f.ph = static_cast<float>(frng_.next()) * 6.28f;
      f.wave = 0;
      f.q = 0;
      f.qv = -3;
      if (shelf_n_ < kShelfSlots) shelf_[shelf_n_++] = f;
      new_egg();
      egg_.x = 380;
      egg_.roll = 0;
      phase_ = Phase::ROLL;
      t_ = 0;
    }
  } else if (phase_ == Phase::ROLL) {
    const float k = phase_k(t_, 0.9);
    egg_.x = lerp(380, kEggX, ease_out_cubic(k));
    egg_.roll = -(egg_.x - kEggX) / kEggRx;
    if (k >= 1) {
      egg_.x = kEggX;
      egg_.roll = 0;
      egg_.qv = -3;
      play(Cue::NOTE, cue::kLand);
      start_egg_play();
    }
  }

  // egg motion
  const float dtf = static_cast<float>(dt);
  spring(egg_, 0, dtf);
  if (egg_.wob > 0) {
    egg_.wob = egg_.wob - dt > 0 ? egg_.wob - dt : 0;
    const float k = 1 - static_cast<float>(egg_.wob / 0.5), amp = hint_ >= 1 ? 0.09f : 0.05f;
    egg_.ang = std::sin(k * kPi * 4) * amp * (1 - k);
  } else {
    egg_.ang = 0;
  }
  if (phase_ == Phase::PRE_HATCH) {
    const float t = static_cast<float>(t_);
    egg_.ang = std::sin(t * 70) * 0.07f * (t / 0.4f + 0.3f);
  }
  egg_.blink -= dt;
  if (egg_.blink < -0.12) egg_.blink = 2 + frng_.next() * 3;

  // shell halves
  if (halves_.on) {
    halves_.t += dt;
    halves_.top_vy += 600 * dtf;
    halves_.top_x += halves_.top_vx * dtf;
    halves_.top_y += halves_.top_vy * dtf;
    halves_.top_a += halves_.top_va * dtf;
    halves_.bot_y = 30 * ease_out_cubic(phase_k(halves_.t - 0.4, 0.8));
    if (halves_.t >= 1.3) halves_.on = false;
  }

  // pet motion
  if (phase_ == Phase::HATCH) pet_.s = lerp(0.2f, 1, ease_out_back(phase_k(t_ - 0.05, 0.5)));
  else if (phase_ == Phase::PET) pet_.s = 1;
  if (pet_.react >= 0) {
    pet_.rt -= dt;
    if (pet_.react == 1 && pet_.rt <= 0.55 && pet_.rt + dt > 0.55) pet_.qv = 4;   // take off: stretch
    if (pet_.react == 1 && pet_.rt <= 0.12 && pet_.rt + dt > 0.12) pet_.qv = -4;  // land: squash
    if (pet_.react == 2 && pet_.rt <= 0.45 && pet_.rt + dt > 0.45) {             // a-choo!
      pet_.qv = -5;
      emit(pet_.x, pet_.y + 2, 10, EmitOpts{40, 110, 0.35f, 0.7f, 60, ParticleShape::STAR, 5, 0, kSparkleColors, 2, true});
    }
    if (pet_.rt <= 0) pet_.react = -1;
  }
  const float sneeze_in = pet_.react == 2 && pet_.rt > 0.45 ? 0.1f * phase_k(0.8 - pet_.rt, 0.35) : 0;
  spring(pet_, sneeze_in, dtf);
  pet_.blink -= dt;
  if (pet_.blink < -0.12) pet_.blink = 2 + frng_.next() * 4;
  for (std::size_t i = 0; i < shelf_n_; ++i) {
    Friend& f = shelf_[i];
    f.ph += dtf;
    f.wave = f.wave - dtf > 0 ? f.wave - dtf : 0;
    spring(f, 0, dtf);
    f.x += (f.tx - f.x) * (dtf * 6 < 1 ? dtf * 6 : 1);
  }

  choices_.out = choices_.out - dt > 0 ? choices_.out - dt : 0;
  badge_.happy = badge_.happy - dt > 0 ? badge_.happy - dt : 0;

  std::size_t n = 0;
  for (std::size_t i = 0; i < part_n_; ++i) {
    Particle p = parts_[i];
    p.life -= dtf;
    p.vy += p.g * dtf;
    p.x += p.vx * dtf;
    p.y += p.vy * dtf;
    p.rot += p.vr * dtf;
    if (p.life > 0) parts_[n++] = p;
  }
  part_n_ = n;
  n = 0;
  for (std::size_t i = 0; i < ring_n_; ++i) {
    RingFx r = rings_[i];
    r.t += dtf;
    if (r.t < r.dur) rings_[n++] = r;
  }
  ring_n_ = n;
  if (decor_n_ < 3 && frng_.next() < dt * 0.5) spawn_fluff(-10, 80 + static_cast<float>(frng_.next()) * 90);
  n = 0;
  for (std::size_t i = 0; i < decor_n_; ++i) {
    Decor d = decor_[i];
    d.ph += dtf;
    d.x += d.vx * dtf;
    d.y += std::sin(d.ph * 1.7f) * 0.25f;
    if (d.x < W + 20) decor_[n++] = d;
  }
  decor_n_ = n;
  if (audio_ && frng_.next() < dt * 0.15) {  // birdSound(): v = 0.1 + frng() * 0.05
    const int q = static_cast<int>(frng_.next() * 15 + 0.5);
    play(Cue::NOTE, static_cast<uint8_t>(cue::kBird | q));
  }
}

void HatchAPetGame::frame_drawn() {
  // drawBadge(): `badge.sv *= 0.86` once per drawn 60 Hz frame (the badge is
  // hidden in START and INTRO). Time-based so slower device frames decay alike.
  if (phase_ == Phase::START || phase_ == Phase::INTRO) return;
  badge_.sv *= std::pow(0.86f, static_cast<float>(last_dt_) * 60.f);
}

void HatchAPetGame::pointer_down(double xd, double yd) {
  const float x = static_cast<float>(xd), y = static_cast<float>(yd);
  egg_.look_x = x - kEggX;
  egg_.look_y = y - kEggCy;
  pet_.look_x = x - pet_.x;
  pet_.look_y = y - pet_.y;
  if (phase_ == Phase::START) {
    start_game();
    return;
  }
  // Home and Mic keys: on device the SceneManager consumes them first; kept
  // for parity with the reference.
  if (in_rect(x, y, 0, 0, 56, 56)) {
    home();
    return;
  }
  if (in_rect(x, y, 264, 0, 56, 56)) {
    add_ring(292, 28, 6, 24, 0.4f, 0xFFFFFF, 2);
    return;
  }
  if (phase_ == Phase::INTRO) {
    t_ = 99;  // intro skippable
    return;
  }
  if (phase_ == Phase::PRE_HATCH || phase_ == Phase::HATCH || phase_ == Phase::BYE || phase_ == Phase::ROLL) return;
  if (in_rect(x, y, kPromptBadge.x, kPromptBadge.y, kPromptBadge.w, kPromptBadge.h)) {
    tap_badge();
    return;
  }
  if (phase_ == Phase::EGG) {
    if (in_rect(x, y, kEggHit.x, kEggHit.y, kEggHit.w, kEggHit.h)) {
      if (time_ - egg_.last_tap >= kTapDebounce) {
        egg_.last_tap = time_;
        knock();
      }
      return;
    }
  } else if (phase_ == Phase::PET) {
    if (choices_.show && choices_.t > kMaxInputBlock) {
      if (std::hypot(x - kYesX, y - kYesY) < kChoiceHit) {
        choose_yes();
        return;
      }
      if (std::hypot(x - kNoX, y - kNoY) < kChoiceHit) {
        choose_no();
        return;
      }
    }
    if (std::hypot(x - pet_.x, y - pet_.y) < kPetHit) {
      if (time_ - pet_.last_tap >= kTapDebounce) {
        pet_.last_tap = time_;
        react();
      }
      return;
    }
  }
  for (std::size_t i = 0; i < shelf_n_; ++i) {
    Friend& f = shelf_[i];
    if (std::hypot(x - f.x, y - f.y) < 16) {
      f.wave = 0.6f;
      play(Cue::NOTE, static_cast<uint8_t>(cue::kShelf | static_cast<int>(f.c)));
      return;
    }
  }
  add_ring(x, y, 3, 22, 0.5f, 0xFFFFFF, 2);
  emit(x, y, 4, EmitOpts{40, 110, 0.35f, 0.7f, 60, ParticleShape::STAR, 5, 0, kSparkleColors, 2, true});
  play(Cue::NOTE, cue::kTap);
  idle_ = 0;
}

void HatchAPetGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  if (event.source == Source::TOUCH) {
    pointer_down(event.x, event.y);
    return;
  }
  // Voice (not in the touch-only reference): same outcomes as the touch path.
  if (phase_ == Phase::INTRO) {
    t_ = 99;
    return;
  }
  const bool any = event.intent == Intent::YES || event.intent == Intent::NO || event.intent == Intent::NEXT ||
                   event.intent == Intent::AGAIN || event.intent == Intent::HELP;
  if (!any) return;
  if (phase_ == Phase::EGG) {
    if (event.intent == Intent::AGAIN || event.intent == Intent::HELP) {
      tap_badge();
    } else if (time_ - egg_.last_tap >= kTapDebounce) {
      egg_.last_tap = time_;
      knock();
    }
  } else if (phase_ == Phase::PET) {
    if (event.intent == Intent::AGAIN || event.intent == Intent::HELP) {
      tap_badge();
      return;
    }
    if (choices_.show && choices_.t > kMaxInputBlock) {
      if (event.intent == Intent::NO) choose_no();
      else choose_yes();
      return;
    }
    if (time_ - pet_.last_tap >= kTapDebounce) {
      pet_.last_tap = time_;
      react();
    }
  }
}

}  // namespace little_arcade::hatch_a_pet
