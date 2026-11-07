#pragma once
#include <cmath>
#include <cstdint>

// DSP pieces for the reference-port game scores (HatchAPetScore): one sine
// table for every voice, and the Web Audio DynamicsCompressor static curve.
// Float only (ESP32-S3 has a single-precision FPU).
namespace little_arcade::audio::score_dsp {

inline constexpr float kPi = 3.14159265358979f;

struct SineTable {
  static constexpr int kSize = 1024;
  float v[kSize + 1];
  SineTable() {
    for (int i = 0; i <= kSize; ++i) v[i] = std::sin(2.f * kPi * static_cast<float>(i) / kSize);
  }
};

// One instance for the whole image (C++17 inline variable).
inline const SineTable kSine;

// 32-bit phase -> sine, linear interpolation between table entries.
inline float sine_at(uint32_t phase) {
  const uint32_t i = phase >> 22;
  const float frac = static_cast<float>(phase & 0x3FFFFFu) * (1.f / 4194304.f);
  return kSine.v[i] + (kSine.v[i + 1] - kSine.v[i]) * frac;
}

// Compressor: threshold -14 dB, ratio 10, knee 30 dB (Web Audio default),
// with Web Audio's automatic makeup gain.
inline constexpr float kThresholdDb = -14.f, kKneeDb = 30.f, kRatio = 10.f;

inline float curve_db(float x) {
  if (x <= kThresholdDb) return x;
  const float d = x - kThresholdDb;
  if (d < kKneeDb) return x + (1.f / kRatio - 1.f) * d * d / (2.f * kKneeDb);
  return kThresholdDb + kKneeDb * (1.f + 1.f / kRatio) * 0.5f + (d - kKneeDb) / kRatio;
}

inline float makeup_db() { return -0.6f * curve_db(0.f); }

}  // namespace little_arcade::audio::score_dsp
