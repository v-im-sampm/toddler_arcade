#pragma once
#include <cmath>
#include <cstdint>

#include "little_arcade/ui/tokens.hpp"

namespace little_arcade::ui::motion {

// Motion helpers shared by every game view (LVGL-free, host-tested). Views
// keep their own animation state and drive it from the game's clock.
inline constexpr float kPi = 3.14159265f;

inline float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float ease_out_cubic(float t) {
  t = clamp01(t);
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}
inline float ease_in_out(float t) {
  t = clamp01(t);
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}
// Overshoots by ~10% before settling: arrivals, pop-ins.
inline float ease_out_back(float t) {
  t = clamp01(t);
  constexpr float c1 = 1.70158f, c3 = c1 + 1.0f;
  const float u = t - 1.0f;
  return 1.0f + c3 * u * u * u + c1 * u * u;
}
// Sine of a phase that advances `1 / period_ms` per millisecond.
inline float wave(uint32_t now_ms, uint32_t period_ms, float phase = 0.0f) {
  const float p = static_cast<float>(now_ms % period_ms) / static_cast<float>(period_ms);
  return std::sin((p + phase) * 2.0f * kPi);
}
// Idle breathing: returns the x-scale offset (y gets the opposite sign).
inline float breath(uint32_t now_ms, float phase = 0.0f, float amount = tok::kBreathScale) {
  return wave(now_ms, tok::kBreathPeriodMs, phase) * amount;
}
// Gentle vertical bob in pixels.
inline float bob(uint32_t now_ms, float phase = 0.0f, float px = tok::kBobPx, uint32_t period_ms = 3000) {
  return wave(now_ms, period_ms, phase) * px;
}
// Hint glow 0..1 (0.9 s period, well under the 3 flashes/s safety limit).
inline float glow(uint32_t now_ms) { return 0.5f + 0.5f * wave(now_ms, tok::kGlowPeriodMs); }
// "Not me" wiggle angle in degrees for `elapsed_ms` since it started; 0 when done.
inline float wiggle_deg(uint32_t elapsed_ms, uint32_t duration_ms = tok::kWiggleMs) {
  if (elapsed_ms >= duration_ms) return 0.0f;
  const float t = static_cast<float>(elapsed_ms) / static_cast<float>(duration_ms);
  return std::sin(t * kPi * 6.0f) * tok::kWiggleDeg * (1.0f - t);
}

// Damped spring (semi-implicit Euler, two substeps: stable at 30-60 Hz).
// Use for squash/stretch and press feedback: kick() on input, step() per frame.
struct Spring {
  float x = 1.0f;
  float v = 0.0f;
  void kick(float dv) { v += dv; }
  void snap(float value) {
    x = value;
    v = 0.0f;
  }
  void step(float dt_s, float target, float k = tok::kSpringBouncyK, float c = tok::kSpringBouncyC) {
    if (dt_s > 0.05f) dt_s = 0.05f;
    const float h = dt_s * 0.5f;
    for (int i = 0; i < 2; ++i) {
      const float a = -k * (x - target) - c * v;
      v += a * h;
      x += v * h;
    }
  }
  bool settled(float target, float eps = 0.002f) const {
    return std::fabs(x - target) < eps && std::fabs(v) < eps * 10.0f;
  }
};

// Cosmetic random numbers (xorshift32). Never use for gameplay decisions:
// games own their seeded RNG so replays stay deterministic.
struct Rng {
  uint32_t s = 0x9E3779B9u;
  explicit Rng(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 0x9E3779B9u) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
  float range(float a, float b) { return a + (b - a) * uniform(); }
};

// Millisecond clock delta helper for views (guards the first frame).
struct Clock {
  uint32_t last = 0;
  bool have = false;
  float dt(uint32_t now_ms) {
    const float d = have ? static_cast<float>(now_ms - last) * 0.001f : 0.0f;
    have = true;
    last = now_ms;
    return d > 0.1f ? 0.1f : d;
  }
  void reset() { have = false; }
};

}  // namespace little_arcade::ui::motion
