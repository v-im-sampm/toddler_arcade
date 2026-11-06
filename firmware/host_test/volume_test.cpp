#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/volume.hpp"

using namespace little_arcade;

namespace {

int max_abs(const int16_t* s, std::size_t n) {
  int m = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const int v = std::abs(static_cast<int>(s[i]));
    if (v > m) m = v;
  }
  return m;
}

void fill_full_scale(int16_t* s, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) s[i] = (i % 2) ? INT16_MAX : INT16_MIN;
}

void test_starts_muted() {
  VolumeLimiter v;
  int16_t buf[256];
  fill_full_scale(buf, 256);
  v.process(buf, 256);
  EXPECT(max_abs(buf, 256) == 0);
  EXPECT(v.level() == 0 && v.current_gain_q15() == 0);
}

void test_parent_cap_clamps_level() {
  VolumeLimiter v;
  EXPECT(v.parent_cap() == VolumeLimiter::kDefaultParentCap);
  v.set_level(10);
  EXPECT(v.level() == VolumeLimiter::kDefaultParentCap);
  v.set_parent_cap(3);
  EXPECT(v.level() == 3);
  v.set_parent_cap(200);
  EXPECT(v.parent_cap() == VolumeLimiter::kMaxLevel && v.level() == 3);
}

void test_ramp_has_no_jump() {
  VolumeLimiter v;
  v.set_level(VolumeLimiter::kDefaultParentCap);
  int16_t buf[64];
  fill_full_scale(buf, 64);
  v.process(buf, 64);
  EXPECT(max_abs(buf, 64) < 1000);  // first 4 ms stays quiet
  EXPECT(v.current_gain_q15() < v.target_gain_q15());
  int16_t big[16000];
  for (int block = 0; block < 2; ++block) {
    fill_full_scale(big, 16000);
    v.process(big, 16000);
  }
  EXPECT(v.current_gain_q15() == v.target_gain_q15());
}

void test_hard_limit_never_exceeded() {
  VolumeLimiter::Config cfg;
  cfg.parent_cap = VolumeLimiter::kMaxLevel;
  VolumeLimiter v(cfg);
  v.set_level(VolumeLimiter::kMaxLevel);
  int16_t buf[16000];
  for (int block = 0; block < 3; ++block) {
    fill_full_scale(buf, 16000);
    v.process(buf, 16000);
    EXPECT(max_abs(buf, 16000) <= VolumeLimiter::kPeakLimit);
  }
  EXPECT(max_abs(buf, 16000) > 0);
}

void test_levels_are_monotonic() {
  for (uint8_t l = 1; l <= VolumeLimiter::kMaxLevel; ++l) {
    EXPECT(VolumeLimiter::gain_for_level(l) > VolumeLimiter::gain_for_level(static_cast<uint8_t>(l - 1)));
  }
  EXPECT(VolumeLimiter::gain_for_level(0) == 0);
  EXPECT(VolumeLimiter::gain_for_level(255) == VolumeLimiter::kMaxGainQ15);
}

void test_mute_ramps_down_to_silence() {
  VolumeLimiter v;
  v.set_level(5);
  int16_t buf[16000];
  fill_full_scale(buf, 16000);
  v.process(buf, 16000);
  v.mute();
  fill_full_scale(buf, 16000);
  v.process(buf, 16000);
  EXPECT(v.current_gain_q15() == 0);
  EXPECT(buf[15999] == 0);
}

std::vector<int32_t> sine(std::size_t n, int32_t amplitude, double hz) {
  std::vector<int32_t> s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<int32_t>(std::lround(amplitude * std::sin(6.283185307179586 * hz * i / 16000.0)));
  return s;
}

// A mix louder than int16 full scale comes out as a scaled sine under the
// ceiling, not a flat-topped (clipped) one.
void test_loud_mix_is_limited_without_clipping() {
  VolumeLimiter v;
  v.set_level(VolumeLimiter::kMaxLevel);
  const std::size_t n = 16000;
  const auto in = sine(n, 60000, 440.0);
  std::vector<int16_t> out(n);
  v.process(in.data(), out.data(), n);
  int peak = 0;
  for (int16_t s : out) peak = std::max(peak, std::abs(static_cast<int>(s)));
  EXPECT(peak <= VolumeLimiter::kPeakLimit);
  EXPECT(peak > VolumeLimiter::kPeakLimit * 9 / 10);
  // Steady state (last 0.25 s): out == in * constant gain.
  const double g = static_cast<double>(peak) / 60000.0;
  int clipped_run = 0, worst_run = 0;
  double worst_err = 0;
  for (std::size_t i = n - 4000; i < n; ++i) {
    worst_err = std::max(worst_err, std::fabs(out[i] - in[i] * g));
    clipped_run = std::abs(out[i]) >= VolumeLimiter::kPeakLimit ? clipped_run + 1 : 0;
    worst_run = std::max(worst_run, clipped_run);
  }
  EXPECT(worst_err < VolumeLimiter::kPeakLimit * 0.01);
  EXPECT(worst_run <= 1);
}

// The board ceiling (amp gain strap) caps output even for full-scale input.
void test_board_ceiling_is_respected() {
  VolumeLimiter::Config cfg;
  cfg.peak_limit = VolumeLimiter::amp_safe_peak(12, 3300);
  VolumeLimiter v(cfg);
  EXPECT(v.peak_limit() == cfg.peak_limit);
  v.set_level(VolumeLimiter::kMaxLevel);
  const auto in = sine(16000, 120000, 150.0);
  std::vector<int16_t> out(16000);
  v.process(in.data(), out.data(), out.size());
  int peak = 0;
  for (int16_t s : out) peak = std::max(peak, std::abs(static_cast<int>(s)));
  EXPECT(peak <= cfg.peak_limit && peak > cfg.peak_limit * 9 / 10);
}

void test_amp_safe_peak_follows_gain_and_supply() {
  // MAX98357A at 12 dB on 3.3 V: 0 dBFS = 7.17 V peak, ~2.8 V usable => ~-8 dBFS.
  const int16_t p12 = VolumeLimiter::amp_safe_peak(12, 3300);
  EXPECT(p12 > 12000 && p12 < 13500);
  EXPECT(VolumeLimiter::amp_safe_peak(15, 3300) < p12);
  EXPECT(VolumeLimiter::amp_safe_peak(9, 3300) == VolumeLimiter::kPeakLimit);
  EXPECT(VolumeLimiter::amp_safe_peak(3, 5000) == VolumeLimiter::kPeakLimit);
  EXPECT(VolumeLimiter::amp_safe_peak(12, 5000) > p12);
  EXPECT(VolumeLimiter::amp_safe_peak(200, 3300) == VolumeLimiter::amp_safe_peak(15, 3300));
}

void test_top_level_is_unity_and_taper_is_perceptual() {
  EXPECT(VolumeLimiter::gain_for_level(VolumeLimiter::kMaxLevel) == VolumeLimiter::kUnityQ15);
  // Each step is about 4 dB (x0.63), so level 1 is still audible but quiet.
  for (uint8_t l = 2; l <= VolumeLimiter::kMaxLevel; ++l) {
    const double r = static_cast<double>(VolumeLimiter::gain_for_level(static_cast<uint8_t>(l - 1))) /
                     VolumeLimiter::gain_for_level(l);
    EXPECT(r > 0.6 && r < 0.66);
  }
}

}  // namespace

int main() {
  test_starts_muted();
  test_parent_cap_clamps_level();
  test_ramp_has_no_jump();
  test_hard_limit_never_exceeded();
  test_levels_are_monotonic();
  test_mute_ramps_down_to_silence();
  test_loud_mix_is_limited_without_clipping();
  test_board_ceiling_is_respected();
  test_amp_safe_peak_follows_gain_and_supply();
  test_top_level_is_unity_and_taper_is_perceptual();
  return little_arcade::testing::finish("volume limiter tests");
}
