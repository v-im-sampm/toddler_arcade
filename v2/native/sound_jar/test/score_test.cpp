// Host tests for SoundJarScore (the Web Audio port) and the shared output limiter path.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "audio/sound_jar_score.hpp"
#include "little_arcade/volume.hpp"

using namespace sound_jar;

static int g_failures = 0;
#define CHECK(cond)                                                                              \
  do {                                                                                           \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
  } while (0)

namespace {
constexpr uint32_t kSr = SoundJarScore::kSampleRate;

struct Stats { int32_t peak = 0; double rms = 0; };
Stats render(SoundJarScore& s, double seconds, std::vector<int32_t>* keep = nullptr) {
  Stats st;
  double sum = 0;
  const std::size_t n = static_cast<std::size_t>(seconds * kSr);
  std::vector<int32_t> buf(256);
  std::size_t done = 0;
  while (done < n) {
    const std::size_t m = std::min<std::size_t>(256, n - done);
    std::fill(buf.begin(), buf.begin() + m, 0);
    s.render_add(buf.data(), m);
    for (std::size_t i = 0; i < m; ++i) {
      st.peak = std::max(st.peak, std::abs(buf[i]));
      sum += static_cast<double>(buf[i]) * buf[i];
      if (keep) keep->push_back(buf[i]);
    }
    done += m;
  }
  st.rms = n ? std::sqrt(sum / n) : 0;
  return st;
}
}  // namespace

static void test_silent_until_start() {
  SoundJarScore s;
  s.cue(Cue::TapNote, 0);
  s.cue(Cue::Tada, 0);
  s.voice_start(1, 1, 1.0, false, false);
  CHECK(render(s, 1.0).peak == 0);
  CHECK(!s.running());
}

static void test_start_is_not_loud() {
  SoundJarScore s;
  s.start();
  CHECK(render(s, 0.5).peak == 0);  // initAudio() alone makes no sound
  s.cue(Cue::Tada, 0);
  const Stats st = render(s, 1.0);
  CHECK(st.peak > 0);
  CHECK(st.peak < 32767);
}

static void test_sampler_voice_plays_take() {
  SoundJarScore s;
  s.start();
  render(s, 0.3);
  s.voice_start(7, 1, 1.0, false, false);
  render(s, 0.01);
  CHECK(s.sampler_voices() == 1);
  const Stats st = render(s, 0.5);
  CHECK(st.peak > 100);
  render(s, 3.0);
  CHECK(s.sampler_voices() == 0);  // one-shot ends
}

static void test_loop_and_release() {
  SoundJarScore s;
  s.start();
  render(s, 0.2);
  s.voice_start(3, 2, 1.0, false, true);
  render(s, 4.0);
  CHECK(s.sampler_voices() == 1);  // still looping past the take length
  s.voice_release(3, 0.05);
  render(s, 0.6);
  CHECK(s.sampler_voices() == 0);
}

static void test_pitch_ratio() {
  // A pad 7 semitones up plays the same take 1.5x faster, so a one-shot ends 1.5x sooner.
  auto measure = [](double rate) {
    SoundJarScore s;
    s.start();
    render(s, 0.3);
    s.voice_start(1, 1, rate, false, false);
    render(s, 0.01);
    int blocks = 0;
    while (s.sampler_voices() > 0 && blocks < 2000) { render(s, 0.004); ++blocks; }
    return blocks * 0.004;
  };
  const double d0 = measure(1.0), d1 = measure(std::pow(2.0, 7 / 12.0));
  CHECK(d0 > 0.3);
  CHECK(std::abs(d0 / d1 - 1.498) < 0.06);
}

static void test_fx_parameters() {
  SoundJarScore s;
  s.start();
  s.apply_fx(true, false, false);
  render(s, 0.6);
  CHECK(s.echo_send() > 0.9f && s.dry_gain() > 0.9f && s.wet_gain() < 0.1f);
  s.apply_fx(false, true, true);
  render(s, 0.6);
  CHECK(s.echo_send() < 0.1f && s.dry_gain() < 0.1f && s.wet_gain() > 0.9f);
  CHECK(s.wobble_depth() > 140.f);
  s.apply_fx(false, false, false);
  render(s, 0.6);
  CHECK(s.dry_gain() > 0.9f && s.wobble_depth() < 10.f);
}

static void test_echo_tail() {
  SoundJarScore s;
  s.start();
  s.apply_fx(true, false, false);
  render(s, 0.6);
  s.cue(Cue::TapNote, 0);
  render(s, 0.05);
  // marimba cues are not on the sampler bus; a take is. Its echo continues after it stops.
  s.voice_start(1, 1, 1.0, false, false);
  render(s, 0.2);
  s.voice_release(1, 0.01);
  render(s, 0.05);
  const Stats tail = render(s, 0.3);
  CHECK(tail.peak > 0);
}

static void test_voice_limit() {
  SoundJarScore s;
  s.start();
  render(s, 0.05);
  for (int i = 0; i < 12; ++i) s.cue(Cue::Tada, 0);
  render(s, 0.05);
  CHECK(s.voice_refusals() > 0);
  CHECK(s.notes() <= SoundJarScore::kMaxNotes);
}

static void test_music_and_duck() {
  SoundJarScore s;
  s.start();
  s.start_music();
  render(s, 4.0);
  CHECK(s.music_gain() > 0.5f);
  s.drip(0, 2);
  CHECK(render(s, 0.5).peak > 0);
  s.duck(0.6);
  render(s, 0.3);
  CHECK(s.duck_gain() < 0.6f);
  render(s, 1.5);
  CHECK(s.duck_gain() > 0.9f);
  s.stop_music();
  render(s, 1.0);
  CHECK(s.music_gain() < 0.05f);
}

static void test_queue_overflow_counts() {
  SoundJarScore s;
  for (int i = 0; i < 300; ++i) s.drip(0, 1);
  CHECK(s.dropped() > 0);
  render(s, 0.1);  // drains without crashing
}

static void test_limiter_bounds_output() {
  SoundJarScore s;
  little_arcade::VolumeLimiter limiter{little_arcade::VolumeLimiter::Config{}};
  limiter.set_level(4);
  s.start();
  s.start_music();
  s.apply_fx(true, false, true);
  std::vector<int32_t> mix(little_arcade::VolumeLimiter::kChunk);
  std::vector<int16_t> out(mix.size());
  int peak = 0;
  for (int k = 0; k < 400; ++k) {
    if (k % 20 == 0) { s.cue(Cue::Tada, 0); s.voice_start(100 + k, k % 4, 1.2, k % 2, false); }
    std::fill(mix.begin(), mix.end(), 0);
    s.render_add(mix.data(), mix.size());
    limiter.process(mix.data(), out.data(), mix.size());
    for (int16_t v : out) peak = std::max(peak, std::abs(static_cast<int>(v)));
  }
  CHECK(peak > 0);
  CHECK(peak < 32767);
}

int main() {
  test_silent_until_start();
  test_start_is_not_loud();
  test_sampler_voice_plays_take();
  test_loop_and_release();
  test_pitch_ratio();
  test_fx_parameters();
  test_echo_tail();
  test_voice_limit();
  test_music_and_duck();
  test_queue_overflow_counts();
  test_limiter_bounds_output();
  if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
  std::puts("sound_jar_score_test: all passed");
  return 0;
}
