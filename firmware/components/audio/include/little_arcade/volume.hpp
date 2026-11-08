#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade {

// Speaker-protection policy applied to every PCM block before I2S TX
// (AGENTS.md rule 9). Starts muted, ramps rather than jumping, honours a
// parent cap, and limits sample peaks regardless of the asset level.
//
// Input is the unclamped int32 mix of all voices, so overlapping sounds are
// never hard-clipped before the volume is applied. Peaks above the ceiling
// are pulled down by a smooth look-ahead limiter (gain falls ahead of the
// peak, holds, then releases slowly) instead of being flat-topped.
//
// The ceiling should come from amp_safe_peak() for the fitted amplifier gain
// strap and supply; those are owner-reported until H03 bench measurements,
// so the values are provisional digital ceilings, not acoustic guarantees.
class VolumeLimiter {
 public:
  // Parent-selected volume scale: 0 = mute, kMaxLevel = unity.
  static constexpr uint8_t kMaxLevel = 6;
  static constexpr uint8_t kDefaultParentCap = kMaxLevel;
  static constexpr int32_t kUnityQ15 = 32768;
  // Digital gain at level kMaxLevel; the limiter keeps it under the ceiling.
  static constexpr int32_t kMaxGainQ15 = kUnityQ15;
  // Absolute output ceiling (about -6 dBFS); per-board ceilings only go lower.
  static constexpr int16_t kPeakLimit = 16384;
  // Samples processed per internal chunk (look-ahead window).
  static constexpr std::size_t kChunk = 256;
  static constexpr uint32_t kAttackMs = 2;
  static constexpr uint32_t kHoldMs = 30;  // > half period of the lowest tone (50 Hz)
  static constexpr uint32_t kReleaseMs = 250;

  struct Config {
    uint8_t parent_cap = kDefaultParentCap;
    uint32_t sample_rate_hz = 16000;
    uint32_t ramp_ms = 300;          // time to move across the full gain range
    int16_t peak_limit = kPeakLimit;  // clamped to kPeakLimit
  };

  // Highest digital peak that keeps a MAX98357A-class amp out of supply
  // clipping. Datasheet: 0 dBFS = 2.1 dBV (1.80 V peak) + strap gain (3, 6,
  // 9, 12 or 15 dB; unknown values use 15 dB). Usable bridge-tied swing is
  // taken as 85% of the supply to allow for output drop and rail sag.
  static constexpr int16_t amp_safe_peak(uint8_t amp_gain_db, uint32_t supply_mv) {
    const uint32_t fs_peak_mv = amp_gain_db <= 3    ? 2544u
                                : amp_gain_db <= 6  ? 3593u
                                : amp_gain_db <= 9  ? 5076u
                                : amp_gain_db <= 12 ? 7170u
                                                    : 10128u;
    const uint64_t peak = static_cast<uint64_t>(32767u) * (supply_mv * 85u / 100u) / fs_peak_mv;
    return peak > static_cast<uint64_t>(kPeakLimit) ? kPeakLimit : static_cast<int16_t>(peak);
  }

  VolumeLimiter() : VolumeLimiter(Config{}) {}
  explicit VolumeLimiter(Config config);

  // Requested level is clamped to the parent cap. Level 0 mutes.
  void set_level(uint8_t level);
  void set_parent_cap(uint8_t cap);
  void mute() { set_level(0); }

  uint8_t level() const { return level_; }
  uint8_t parent_cap() const { return cap_; }
  int16_t peak_limit() const { return ceiling_; }
  int32_t target_gain_q15() const { return target_gain_; }
  int32_t current_gain_q15() const { return gain_; }
  int32_t limiter_gain_q15() const { return lim_gain_; }

  // Volume gain + ramp + peak limiter: in[0..count) -> out[0..count).
  void process(const int32_t* in, int16_t* out, std::size_t count);
  // In-place convenience for int16 sources.
  void process(int16_t* samples, std::size_t count);

  // Perceptual taper: 4 dB per step, level kMaxLevel = unity, 0 = mute.
  static constexpr int32_t gain_for_level(uint8_t level) {
    constexpr int32_t kTable[kMaxLevel + 1] = {0, 3277, 5193, 8231, 13045, 20675, kMaxGainQ15};
    return kTable[level >= kMaxLevel ? kMaxLevel : level];
  }

 private:
  void process_chunk(const int32_t* in, int16_t* out, std::size_t n);

  uint8_t cap_;
  int16_t ceiling_;
  uint8_t level_ = 0;
  int32_t target_gain_ = 0;
  int32_t gain_ = 0;  // starts muted: no loud startup sound
  int32_t step_per_sample_;
  int32_t lim_gain_ = kUnityQ15;
  int32_t attack_step_;
  int32_t release_step_;
  uint32_t hold_samples_;
  uint32_t hold_ = 0;
  int32_t scratch_[kChunk];
  int32_t env_[kChunk];
};

}  // namespace little_arcade
