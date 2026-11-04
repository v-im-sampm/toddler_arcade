#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade {

// Speaker-protection policy applied to every PCM block before I2S TX
// (AGENTS.md rule 9). Starts muted, ramps rather than jumping, honours a
// parent cap, and hard-limits sample peaks regardless of the asset level.
// The final analog level also depends on the amplifier gain strap and the
// speaker, which are TBD until H03 bench measurements; constants here are
// provisional digital ceilings, not acoustic guarantees.
class VolumeLimiter {
 public:
  static constexpr uint8_t kMaxLevel = 10;
  static constexpr uint8_t kDefaultParentCap = 6;
  static constexpr int32_t kUnityQ15 = 32768;
  // Full-scale digital gain at level kMaxLevel (0.5 = -6 dBFS headroom).
  static constexpr int32_t kMaxGainQ15 = kUnityQ15 / 2;
  // Absolute output ceiling after gain (about -6 dBFS).
  static constexpr int16_t kPeakLimit = 16384;

  struct Config {
    uint8_t parent_cap = kDefaultParentCap;
    uint32_t sample_rate_hz = 16000;
    uint32_t ramp_ms = 300;  // time to move across the full gain range
  };

  VolumeLimiter() : VolumeLimiter(Config{}) {}
  explicit VolumeLimiter(Config config);

  // Requested level is clamped to the parent cap. Level 0 mutes.
  void set_level(uint8_t level);
  void set_parent_cap(uint8_t cap);
  void mute() { set_level(0); }

  uint8_t level() const { return level_; }
  uint8_t parent_cap() const { return cap_; }
  int32_t target_gain_q15() const { return target_gain_; }
  int32_t current_gain_q15() const { return gain_; }

  // In-place gain + ramp + hard limit.
  void process(int16_t* samples, std::size_t count);

  static constexpr int32_t gain_for_level(uint8_t level) {
    return level >= kMaxLevel ? kMaxGainQ15 : (kMaxGainQ15 * level) / kMaxLevel;
  }

 private:
  uint8_t cap_;
  uint8_t level_ = 0;
  int32_t target_gain_ = 0;
  int32_t gain_ = 0;  // starts muted: no loud startup sound
  int32_t step_per_sample_;
};

}  // namespace little_arcade
