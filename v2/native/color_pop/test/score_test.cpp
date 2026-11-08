// Host tests for ColorPopScore (the Web Audio port) and the shared output limiter path.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "audio/color_pop_score.hpp"
#include "little_arcade/volume.hpp"

using namespace color_pop;

static int g_failures = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
  } while (0)

namespace {
constexpr uint32_t kSr = ColorPopScore::kSampleRate;

struct Stats { int32_t peak = 0; double rms = 0; };
Stats render(ColorPopScore& s, double seconds, std::vector<int32_t>* keep = nullptr) {
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
  ColorPopScore s;
  s.marimba(523.25, 0, 1, 0.55);  // reference: if (!ac) return
  s.plop(0, 1, 1);
  s.motif(Color::Red, 0);
  Stats st = render(s, 1.0);
  CHECK(st.peak == 0);
  CHECK(!s.running());
  CHECK(s.notes() == 0);
}

static void test_soft_start_ramp() {
  ColorPopScore s;
  s.start();
  std::vector<int32_t> out;
  render(s, 1.0, &out);
  CHECK(s.running());
  int32_t first = 0, later = 0;
  for (std::size_t i = 0; i < 160; ++i) first = std::max(first, std::abs(out[i]));         // 10 ms
  for (std::size_t i = 8000; i < 16000; ++i) later = std::max(later, std::abs(out[i]));
  CHECK(first < 200);         // master gain ramps from 0 (tau 0.1 s): no loud start
  CHECK(later > 0);           // the wave bed is audible
  CHECK(later < 2500);        // and quiet (gain 0.018-0.032 of a 0.35 master)
}

static void test_notes_and_bound() {
  ColorPopScore s;
  s.start();
  render(s, 0.5);
  s.plop(0, 1, 1);
  s.marimba(degree_hz(0), 0.03, 1, 0.55);
  Stats st = render(s, 0.7);
  CHECK(st.peak > 3000);
  CHECK(st.peak < 32767);
  render(s, 0.2);
  CHECK(s.notes() == 0);  // marimba stops at dur + 0.05
}

static void test_voice_limit() {
  ColorPopScore s;
  s.start();
  render(s, 0.05);
  for (int i = 0; i < 12; ++i) s.marimba(degree_hz(i % 5), 0, 1, 0.55);
  Stats st = render(s, 0.05);
  CHECK(s.notes() == 8);
  CHECK(s.voice_refusals() == 4);
  CHECK(st.peak < 32767);
  render(s, 0.6);
  for (int i = 0; i < 4; ++i) s.plop(0, 1, 1);  // earlier voices expired
  render(s, 0.01);
  CHECK(s.voice_refusals() == 4);
}

static void test_delayed_motif_and_celebrate() {
  ColorPopScore s;
  s.start();
  render(s, 0.1);
  s.motif(Color::Yellow, 0.25);
  render(s, 0.2);
  CHECK(s.notes() == 0);  // setTimeout(250 ms) not yet elapsed
  render(s, 0.1);
  CHECK(s.notes() == 4);  // yellow motif = 4 notes scheduled at once
  render(s, 1.2);
  s.celebrate(true, 0.18);
  render(s, 0.2);
  CHECK(s.notes() == 8);  // big arpeggio: 8 notes
  render(s, 2.0);
  s.celebrate(false, 0);
  render(s, 0.01);
  CHECK(s.notes() == 4);
}

static void test_mashing_queue() {
  ColorPopScore s;
  s.start();
  for (int i = 0; i < 200; ++i) s.plop(0, 1, 1);  // more than the 64-entry ring between renders
  CHECK(s.dropped() == 200 - ColorPopScore::kQueue + 1 || s.dropped() == 200 - ColorPopScore::kQueue);
  Stats st = render(s, 0.3);
  CHECK(st.peak < 32767);
  CHECK(s.notes() == 0 || s.notes() <= ColorPopScore::kMaxVoices);
}

static void test_limiter_path() {
  // Desktop/device path: score -> int32 mix -> VolumeLimiter -> int16.
  ColorPopScore s;
  little_arcade::VolumeLimiter::Config cfg{};
  little_arcade::VolumeLimiter limiter(cfg);
  limiter.set_level(3);
  s.start();
  std::vector<int32_t> mix(256);
  std::vector<int16_t> out(256);
  int peak = 0;
  for (int block = 0; block < 200; ++block) {
    if (block == 20) { s.celebrate(true, 0); s.plop(0, 1, 1); }
    std::fill(mix.begin(), mix.end(), 0);
    s.render_add(mix.data(), mix.size());
    limiter.process(mix.data(), out.data(), out.size());
    for (int16_t v : out) peak = std::max(peak, std::abs(static_cast<int>(v)));
  }
  CHECK(peak > 0);
  CHECK(peak <= cfg.peak_limit);
}

int main() {
  test_silent_until_start();
  test_soft_start_ramp();
  test_notes_and_bound();
  test_voice_limit();
  test_delayed_motif_and_celebrate();
  test_mashing_queue();
  test_limiter_path();
  if (g_failures) { std::printf("color_pop_score_test: %d failure(s)\n", g_failures); return 1; }
  std::printf("color_pop_score_test: all checks passed\n");
  return 0;
}
