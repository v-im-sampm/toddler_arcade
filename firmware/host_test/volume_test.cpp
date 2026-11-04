#include <cstdlib>

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

}  // namespace

int main() {
  test_starts_muted();
  test_parent_cap_clamps_level();
  test_ramp_has_no_jump();
  test_hard_limit_never_exceeded();
  test_levels_are_monotonic();
  test_mute_ramps_down_to_silence();
  return little_arcade::testing::finish("volume limiter tests");
}
