#include "little_arcade/volume.hpp"

namespace little_arcade {

namespace {
int32_t step_for(uint32_t sample_rate_hz, uint32_t ms, int32_t range) {
  const uint64_t samples = static_cast<uint64_t>(sample_rate_hz) * (ms ? ms : 1) / 1000u;
  const int32_t step = static_cast<int32_t>(range / static_cast<int64_t>(samples ? samples : 1));
  return step > 0 ? step : 1;
}
}  // namespace

VolumeLimiter::VolumeLimiter(Config config)
    : cap_(config.parent_cap > kMaxLevel ? kMaxLevel : config.parent_cap),
      ceiling_(config.peak_limit <= 0 ? 1 : (config.peak_limit > kPeakLimit ? kPeakLimit : config.peak_limit)) {
  step_per_sample_ = step_for(config.sample_rate_hz, config.ramp_ms, kMaxGainQ15);
  attack_step_ = step_for(config.sample_rate_hz, kAttackMs, kUnityQ15);
  release_step_ = step_for(config.sample_rate_hz, kReleaseMs, kUnityQ15);
  hold_samples_ = static_cast<uint32_t>(static_cast<uint64_t>(config.sample_rate_hz) * kHoldMs / 1000u);
}

void VolumeLimiter::set_level(uint8_t level) {
  level_ = level > cap_ ? cap_ : level;
  target_gain_ = gain_for_level(level_);
}

void VolumeLimiter::set_parent_cap(uint8_t cap) {
  cap_ = cap > kMaxLevel ? kMaxLevel : cap;
  set_level(level_);
}

void VolumeLimiter::process(const int32_t* in, int16_t* out, std::size_t count) {
  while (count > 0) {
    const std::size_t n = count < kChunk ? count : kChunk;
    process_chunk(in, out, n);
    in += n;
    out += n;
    count -= n;
  }
}

void VolumeLimiter::process(int16_t* samples, std::size_t count) {
  int32_t wide[kChunk];
  while (count > 0) {
    const std::size_t n = count < kChunk ? count : kChunk;
    for (std::size_t i = 0; i < n; ++i) wide[i] = samples[i];
    process_chunk(wide, samples, n);
    samples += n;
    count -= n;
  }
}

void VolumeLimiter::process_chunk(const int32_t* in, int16_t* out, std::size_t n) {
  // 1. Volume gain with its click-free ramp, kept wide (no clamping yet).
  for (std::size_t i = 0; i < n; ++i) {
    if (gain_ < target_gain_) {
      gain_ = (target_gain_ - gain_ > step_per_sample_) ? gain_ + step_per_sample_ : target_gain_;
    } else if (gain_ > target_gain_) {
      gain_ = (gain_ - target_gain_ > step_per_sample_) ? gain_ - step_per_sample_ : target_gain_;
    }
    scratch_[i] = static_cast<int32_t>((static_cast<int64_t>(in[i]) * gain_) / kUnityQ15);
  }

  // 2. Gain each sample needs to stay under the ceiling, smeared backwards so
  // the limiter starts easing down kAttackMs before a peak arrives.
  int32_t next = kUnityQ15;
  for (std::size_t k = n; k-- > 0;) {
    const int64_t mag = scratch_[k] < 0 ? -static_cast<int64_t>(scratch_[k]) : scratch_[k];
    int32_t need = mag > ceiling_ ? static_cast<int32_t>((static_cast<int64_t>(ceiling_) * kUnityQ15) / mag)
                                  : kUnityQ15;
    const int32_t eased = next + attack_step_;
    if (eased < need) need = eased;
    env_[k] = need;
    next = need;
  }

  // 3. Follow the envelope down immediately, hold, then release slowly so the
  // gain never wobbles within a waveform cycle (that would itself distort).
  for (std::size_t i = 0; i < n; ++i) {
    if (env_[i] <= lim_gain_) {
      lim_gain_ = env_[i];
      hold_ = hold_samples_;
    } else if (hold_ > 0) {
      --hold_;
    } else {
      lim_gain_ = (env_[i] - lim_gain_ > release_step_) ? lim_gain_ + release_step_ : env_[i];
    }
    int32_t v = static_cast<int32_t>((static_cast<int64_t>(scratch_[i]) * lim_gain_) / kUnityQ15);
    if (v > ceiling_) v = ceiling_;  // safety net for a peak at a chunk boundary
    if (v < -ceiling_) v = -ceiling_;
    out[i] = static_cast<int16_t>(v);
  }
}

}  // namespace little_arcade
