#pragma once
#include <cmath>
#include <cstdint>

#include "little_arcade/ui/motion.hpp"
#include "little_arcade/ui/tokens.hpp"

namespace little_arcade::ui {

// Particle shapes the FX layer can draw (all recoloured white sprites).
enum class FxShape : uint8_t { DOT, STAR, CONFETTI, BUBBLE, HEART, NOTE };

// Emitter preset (v2 tokens.json "particles.presets"). Speeds in px/s,
// gravity in px/s^2 (negative floats upward), lives in seconds.
struct FxEmit {
  float speed_min = 40.0f, speed_max = 110.0f;
  float life_min = 0.35f, life_max = 0.7f;
  float gravity = 60.0f;
  float up = 0.0f;  // extra initial upward speed
  float size = 5.0f;
  FxShape shape = FxShape::DOT;
  uint32_t colors[5] = {tok::kWhite, 0, 0, 0, 0};
  uint8_t ncolors = 1;
  bool additive = false;
  float angle = 0.0f;   // centre direction (radians, y down)
  float spread = 6.2832f;  // full circle by default
};

namespace fx {
inline FxEmit sparkle() {
  FxEmit e;
  e.speed_min = 40;
  e.speed_max = 110;
  e.life_min = 0.35f;
  e.life_max = 0.7f;
  e.gravity = 60;
  e.size = 6;
  e.shape = FxShape::STAR;
  e.colors[0] = tok::kYellow.light;
  e.colors[1] = tok::kCream.light;
  e.ncolors = 2;
  e.additive = true;
  return e;
}
inline FxEmit confetti() {
  FxEmit e;
  e.speed_min = 60;
  e.speed_max = 160;
  e.life_min = 0.9f;
  e.life_max = 1.5f;
  e.gravity = 140;
  e.up = 60;
  e.size = 7;
  e.shape = FxShape::CONFETTI;
  e.colors[0] = tok::kRed.base;
  e.colors[1] = tok::kYellow.base;
  e.colors[2] = tok::kGreen.base;
  e.colors[3] = tok::kBlue.base;
  e.colors[4] = tok::kPink.base;
  e.ncolors = 5;
  return e;
}
// Juicy burst in a hue (pops, splats).
inline FxEmit burst(const tok::Hue& h, float size = 5.0f) {
  FxEmit e;
  e.speed_min = 50;
  e.speed_max = 130;
  e.life_min = 0.3f;
  e.life_max = 0.6f;
  e.gravity = 120;
  e.size = size;
  e.colors[0] = h.base;
  e.colors[1] = h.light;
  e.ncolors = 2;
  return e;
}
// Slow rising outline bubbles (ambient life, water scenes).
inline FxEmit bubbles() {
  FxEmit e;
  e.speed_min = 15;
  e.speed_max = 35;
  e.life_min = 1.2f;
  e.life_max = 2.4f;
  e.gravity = -30;
  e.size = 5;
  e.shape = FxShape::BUBBLE;
  e.colors[0] = tok::kWhite;
  e.ncolors = 1;
  e.angle = -1.5708f;
  e.spread = 1.2f;
  return e;
}
inline FxEmit hearts() {
  FxEmit e;
  e.speed_min = 25;
  e.speed_max = 60;
  e.life_min = 0.8f;
  e.life_max = 1.3f;
  e.gravity = -25;
  e.size = 8;
  e.shape = FxShape::HEART;
  e.colors[0] = tok::kPink.base;
  e.colors[1] = tok::kRed.base;
  e.ncolors = 2;
  e.angle = -1.5708f;
  e.spread = 1.6f;
  return e;
}
inline FxEmit notes() {
  FxEmit e;
  e.speed_min = 20;
  e.speed_max = 50;
  e.life_min = 0.8f;
  e.life_max = 1.2f;
  e.gravity = -20;
  e.size = 9;
  e.shape = FxShape::NOTE;
  e.colors[0] = tok::kPurple.base;
  e.colors[1] = tok::kPink.base;
  e.ncolors = 2;
  e.angle = -1.5708f;
  e.spread = 1.4f;
  return e;
}
}  // namespace fx

struct FxParticle {
  float x = 0, y = 0, vx = 0, vy = 0, g = 0;
  float life = 0, max_life = 1, size = 4, rot = 0, vr = 0;
  uint32_t color = 0;
  uint32_t born = 0;
  FxShape shape = FxShape::DOT;
  bool additive = false;
  bool alive = false;
  float fade() const {
    const float k = life / max_life * 1.6f;
    return k > 1.0f ? 1.0f : (k < 0.0f ? 0.0f : k);
  }
};

// Fixed-capacity particle simulation. Never exceeds N live particles: when
// full the oldest is recycled (same rule as the quality-bar sketch).
template <int N>
class FxSim {
 public:
  static constexpr int kCapacity = N;
  explicit FxSim(uint32_t seed = 7) : rng_(seed) {}

  void clear() {
    for (auto& p : p_) p.alive = false;
  }
  void set_reduced_motion(bool on) { reduced_ = on; }
  bool reduced_motion() const { return reduced_; }

  // Returns the number actually emitted (halved in reduced motion).
  int emit(float x, float y, int n, const FxEmit& e) {
    if (reduced_) n = (n + 1) / 2;
    if (n < 1) n = 1;
    for (int i = 0; i < n; ++i) {
      FxParticle& p = slot();
      const float a = e.angle + (rng_.uniform() - 0.5f) * e.spread;
      const float sp = rng_.range(e.speed_min, e.speed_max);
      p.x = x;
      p.y = y;
      p.vx = std::cos(a) * sp;
      p.vy = std::sin(a) * sp - e.up;
      p.g = e.gravity;
      p.life = p.max_life = rng_.range(e.life_min, e.life_max);
      p.size = e.size * rng_.range(0.7f, 1.3f);
      p.rot = rng_.range(0.0f, 6.28f);
      p.vr = (rng_.uniform() - 0.5f) * 8.0f;
      p.color = e.colors[e.ncolors > 1 ? rng_.next() % e.ncolors : 0];
      p.shape = e.shape;
      p.additive = e.additive;
      p.born = ++births_;
      p.alive = true;
    }
    return n;
  }

  void step(float dt) {
    for (auto& p : p_) {
      if (!p.alive) continue;
      p.life -= dt;
      if (p.life <= 0.0f) {
        p.alive = false;
        continue;
      }
      p.vy += p.g * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.rot += p.vr * dt;
      if (p.shape == FxShape::BUBBLE) p.x += std::sin(p.rot) * 0.3f;  // sway
    }
  }

  int live() const {
    int n = 0;
    for (const auto& p : p_) n += p.alive ? 1 : 0;
    return n;
  }
  const FxParticle& at(int i) const { return p_[i]; }

 private:
  FxParticle& slot() {
    FxParticle* oldest = &p_[0];
    for (auto& p : p_) {
      if (!p.alive) return p;
      if (p.born < oldest->born) oldest = &p;
    }
    return *oldest;
  }

  FxParticle p_[N];
  motion::Rng rng_;
  uint32_t births_ = 0;
  bool reduced_ = false;
};

}  // namespace little_arcade::ui
