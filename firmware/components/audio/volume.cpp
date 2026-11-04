#include "little_arcade/volume.hpp"

namespace little_arcade {

VolumeLimiter::VolumeLimiter(Config config)
    : cap_(config.parent_cap > kMaxLevel ? kMaxLevel : config.parent_cap) {
  const uint64_t ramp_samples =
      static_cast<uint64_t>(config.sample_rate_hz) * (config.ramp_ms ? config.ramp_ms : 1) / 1000u;
  const int32_t step = static_cast<int32_t>(kMaxGainQ15 / (ramp_samples ? ramp_samples : 1));
  step_per_sample_ = step > 0 ? step : 1;
}

void VolumeLimiter::set_level(uint8_t level) {
  level_ = level > cap_ ? cap_ : level;
  target_gain_ = gain_for_level(level_);
}

void VolumeLimiter::set_parent_cap(uint8_t cap) {
  cap_ = cap > kMaxLevel ? kMaxLevel : cap;
  set_level(level_);
}

void VolumeLimiter::process(int16_t* samples, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    if (gain_ < target_gain_) {
      gain_ = (target_gain_ - gain_ > step_per_sample_) ? gain_ + step_per_sample_ : target_gain_;
    } else if (gain_ > target_gain_) {
      gain_ = (gain_ - target_gain_ > step_per_sample_) ? gain_ - step_per_sample_ : target_gain_;
    }
    int32_t v = (static_cast<int32_t>(samples[i]) * gain_) / kUnityQ15;
    if (v > kPeakLimit) v = kPeakLimit;
    if (v < -kPeakLimit) v = -kPeakLimit;
    samples[i] = static_cast<int16_t>(v);
  }
}

}  // namespace little_arcade
