// Host tests for HatchAPetScore, the port of hatch_a_pet.html's Web Audio score.
// Host only: checks pitch, timing, voice limit, ramp-in, the lullaby and loudness bounds;
// it does not prove acoustic level on the speaker.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/hatch_a_pet_score.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::audio;

namespace {

std::size_t ms(double v) { return static_cast<std::size_t>(v * kSampleRateHz / 1000.0); }

// The compressor's 6 ms pre-delay shifts every onset by this much.
constexpr double kDelayMs = 6;

std::vector<int32_t> run(HatchAPetScore& s, std::size_t n) {
  std::vector<int32_t> out(n, 0);
  s.render_add(out.data(), n);
  return out;
}

int32_t peak(const std::vector<int32_t>& v, std::size_t from = 0, std::size_t to = SIZE_MAX) {
  int32_t m = 0;
  for (std::size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs(v[i]));
  return m;
}

double pitch(const std::vector<int32_t>& v, std::size_t from, std::size_t to) {
  int crossings = 0;
  for (std::size_t i = from + 1; i < to; ++i) crossings += (v[i - 1] < 0) != (v[i] < 0) ? 1 : 0;
  return crossings * 0.5 * kSampleRateHz / static_cast<double>(to - from);
}

HatchAPetScore& started(HatchAPetScore& s, double settle_ms = 800) {
  s.cue(Cue::AMBIENT, 0);
  run(s, ms(settle_ms));
  return s;
}

void test_degree_frequencies_match_reference() {
  // degFreq(): 698.46 * 2^((PENT[i] + 12 * (o + oct)) / 12)
  EXPECT(std::fabs(HatchAPetScore::degree_hz(0, 0) - 698.46f) < 0.01f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(0, -1) - 349.23f) < 0.01f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(2, 0) - 880.0f) < 0.1f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(4, 0) - 1174.66f) < 0.2f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(5, 0) - 1396.92f) < 0.2f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(7, 0) - 1760.0f) < 0.3f);
  EXPECT(std::fabs(HatchAPetScore::degree_hz(-1, -1) - 293.66f) < 0.1f);  // floor division
  EXPECT(std::fabs(HatchAPetScore::degree_hz(0, 1) - 1396.92f) < 0.2f);
}

void test_silent_until_ambient() {
  HatchAPetScore s;
  EXPECT(peak(run(s, 4000)) == 0);
  EXPECT(!s.running());
  EXPECT(!s.cue(Cue::TAP, 0));
  EXPECT(!s.cue(Cue::TRY_AGAIN, 0));
  EXPECT(!s.cue(Cue::NOTE, 0x70));
}

void test_master_ramps_in() {
  HatchAPetScore a;
  a.cue(Cue::AMBIENT, 0);
  a.cue(Cue::NOTE, 0x41);
  const int32_t early = peak(run(a, ms(150)));
  HatchAPetScore b;
  started(b, 500);
  b.cue(Cue::NOTE, 0x41);
  const int32_t late = peak(run(b, ms(150)));
  EXPECT(early > 0 && late > early * 2);
}

void test_knock_is_a_woodblock_with_pitch_by_crack() {
  // knockSound(n) = woodblock(degFreq(n - 1, -1)): body glides 0.92x in 50 ms.
  double hz[3];
  for (int n = 1; n <= 3; ++n) {
    HatchAPetScore s;
    started(s);
    s.cue(Cue::NOTE, static_cast<uint8_t>(0x10 | n));
    const auto out = run(s, ms(250));
    EXPECT(peak(out, 0, ms(10)) > 1000);
    EXPECT(peak(out, ms(180), ms(250)) < 1500);  // short: only the soft lullaby remains
    hz[n - 1] = pitch(out, ms(26), ms(51));      // tick has decayed; body gliding f -> 0.92 f
  }
  EXPECT(std::fabs(hz[0] - 349.23 * 0.95) < 25);
  EXPECT(std::fabs(hz[1] - 392.0 * 0.95) < 25);
  EXPECT(std::fabs(hz[2] - 440.0 * 0.95) < 25);
}

void test_knock_knock_later_is_delayed() {
  HatchAPetScore now, later;
  started(now);
  started(later);
  now.cue(Cue::NOTE, 0x20);
  later.cue(Cue::NOTE, 0x21);  // setTimeout(knockKnock, 200)
  const auto a = run(now, ms(700));
  const auto b = run(later, ms(700));
  EXPECT(peak(a, 0, ms(10)) > 1000);
  EXPECT(peak(b, 0, ms(190)) < 1500);
  EXPECT(peak(b, ms(200), ms(220)) > 1000);
  EXPECT(peak(b, ms(420), ms(440)) > 1000);  // second knock 0.22 s later
}

void test_chirp_glides_up() {
  // nameChirp(c): chirp(f) glides f -> 1.5 f over 80 ms.
  HatchAPetScore s;
  started(s);
  s.cue(Cue::REACTION, 0x30 | 2);  // SPROUTLING, deg 2: 880 Hz
  const auto out = run(s, ms(130));
  const double lo = pitch(out, ms(3 + kDelayMs), ms(20 + kDelayMs));
  const double hi = pitch(out, ms(45 + kDelayMs), ms(70 + kDelayMs));  // ~1.32 f mid-glide
  EXPECT(lo > 800 && lo < 1050);
  EXPECT(hi > lo * 1.15 && hi < lo * 1.55);
}

void test_every_cue_sounds_and_differs() {
  const uint8_t notes[] = {0x11, 0x20, 0x30, 0x40, 0x41, 0x42, 0x43, 0x44, 0x53, 0x60, 0x6F};
  const uint8_t reactions[] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x07};
  std::vector<std::vector<int32_t>> all;
  for (uint8_t v : notes) {
    HatchAPetScore s;
    started(s);
    EXPECT(s.cue(Cue::NOTE, v));
    all.push_back(run(s, ms(900)));
    EXPECT(peak(all.back()) > 1500);
  }
  for (uint8_t v : reactions) {
    HatchAPetScore s;
    started(s);
    EXPECT(s.cue(Cue::REACTION, v));
    all.push_back(run(s, ms(900)));
    EXPECT(peak(all.back()) > 1500);
  }
  HatchAPetScore s;
  started(s);
  EXPECT(s.cue(Cue::SUCCESS, 0));
  all.push_back(run(s, ms(1200)));
  for (std::size_t a = 0; a < all.size(); ++a) {
    for (std::size_t b = a + 1; b < all.size(); ++b) EXPECT(all[a] != all[b]);
  }
}

void test_voice_limit_is_eight() {
  HatchAPetScore s;
  started(s, 1);
  s.cue(Cue::NOTE, 0x30);  // trill: 6 marimba notes
  EXPECT(s.notes() == 6);
  s.cue(Cue::NOTE, 0x20);  // knockKnock: 4 glides, only 2 fit
  EXPECT(s.notes() == 8);
  run(s, ms(1500));
  EXPECT(s.notes() == 0);
  s.cue(Cue::NOTE, 0x44);
  EXPECT(s.notes() == 1);
}

void test_hatch_pile_up_stays_bounded() {
  HatchAPetScore s;
  started(s);
  s.cue(Cue::NOTE, 0x30);
  s.cue(Cue::SUCCESS, 0);
  s.cue(Cue::REACTION, 0x21);
  const auto out = run(s, ms(1500));
  EXPECT(peak(out) > 6000);
  EXPECT(peak(out) < 32767 * 2);
}

std::vector<int32_t> lullaby(double seconds) {
  HatchAPetScore s;
  s.cue(Cue::AMBIENT, 0);
  return run(s, ms(seconds * 1000));
}

void test_lullaby_fades_in_and_stays_under_cues() {
  const auto song = lullaby(30);
  // startMusic(): nothing before t0 = 0.5 s, then a slow fade (tau 1.5 s).
  EXPECT(peak(song, 0, ms(500)) == 0);
  const int32_t first = peak(song, ms(500), ms(1500));
  const int32_t full = peak(song, ms(8000), ms(28000));
  EXPECT(first > 0 && first * 3 < full);
  // Quiet bed: well under any single cue (SUCCESS ~ 0.36, a knock ~ 0.38).
  HatchAPetScore s;
  started(s, 4000);
  s.cue(Cue::SUCCESS, 0);
  const int32_t cue = peak(run(s, ms(1200)));
  std::printf("lullaby peak %d vs hatch cue peak %d (%.2f)\n", full, cue, full / static_cast<double>(cue));
  EXPECT(full > 300);
  EXPECT(full * 3 < cue);
  // It keeps looping (20 s song): the second pass is as loud as the first.
  EXPECT(peak(song, ms(20500), ms(28000)) * 10 > full * 8);
}

void test_lullaby_is_deterministic() {
  EXPECT(lullaby(6) == lullaby(6));
}

void test_lullaby_ducks_under_cues() {
  HatchAPetScore s;
  started(s, 6000);
  EXPECT(s.music_gain() > 0.9f);
  s.cue(Cue::REACTION, 0x34);  // nameChirp: voices last 0.12 s
  run(s, ms(100));
  EXPECT(s.music_gain() < 0.4f);
  run(s, ms(1500));
  EXPECT(s.music_gain() > 0.9f);
}

void test_lullaby_has_its_own_voices() {
  HatchAPetScore s;
  started(s, 3000);
  EXPECT(s.music_notes() > 0);
  s.cue(Cue::NOTE, 0x30);  // trill: 6 marimba notes
  s.cue(Cue::NOTE, 0x20);  // knockKnock: only 2 of 4 glides fit
  EXPECT(s.notes() == 8);
  for (int i = 0; i < 400; ++i) {
    run(s, 256);
    EXPECT(s.music_notes() <= HatchAPetScore::kMusicVoices);
  }
}
void test_stop_fades_without_click() {
  HatchAPetScore s;
  started(s, 500);
  s.cue(Cue::SUCCESS, 0);
  const auto before = run(s, ms(100));
  s.stop();
  const auto fade = run(s, ms(300));
  EXPECT(std::abs(fade[0]) <= peak(before));
  EXPECT(peak(fade, ms(200)) == 0);
  EXPECT(!s.running() && s.notes() == 0 && s.music_notes() == 0);
  // Home then a new game: the lullaby starts over with a fresh fade-in.
  s.cue(Cue::AMBIENT, 0);
  EXPECT(peak(run(s, ms(500))) == 0);
  EXPECT(peak(run(s, ms(2000))) > 0);
}

void test_render_cost() {
  // Worst case: 8 cue voices plus the lullaby for one 256-sample device block.
  // Host timing is only a sanity bound; device timing is measured on target.
  HatchAPetScore s;
  started(s, 3000);
  s.cue(Cue::NOTE, 0x30);
  s.cue(Cue::NOTE, 0x20);
  std::vector<int32_t> out(256);
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; ++i) s.render_add(out.data(), out.size());
  const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 100;
  std::printf("host render: %.1f us per 256-sample block (8 voices + lullaby)\n", us);
  EXPECT(us < 2000);
}

// Busy play: intro, knocks, trill, hatch, name chirp, reactions, badge,
// choices, bye - cue times follow script_hatch_a_pet.txt. Rendered in
// 256-sample device blocks through the device limiter chain.
struct BusyStats {
  int32_t score_peak = 0;     // raw score output (int32, before the limiter)
  std::size_t over_ceiling = 0;  // raw samples above the device ceiling
  std::size_t over_full = 0;     // raw samples above int16 full scale
  int min_comp_db = 0;           // deepest compressor gain reduction (dB)
  int32_t min_limiter_q15 = VolumeLimiter::kUnityQ15;
  std::size_t clamped = 0;       // output samples sitting on the ceiling
  int32_t max_step = 0;          // largest sample-to-sample jump in the output
  double dc = 0;                 // mean of the output
};

BusyStats busy_run() {
  struct At { double s; Cue c; uint8_t v; };
  const At seq[] = {
      {0.0, Cue::AMBIENT, 0},     {0.05, Cue::NOTE, 0x41},    {1.2, Cue::NOTE, 0x40},   {1.2, Cue::NOTE, 0x21},
      {2.3, Cue::NOTE, 0x11},     {2.8, Cue::NOTE, 0x12},     {3.3, Cue::NOTE, 0x13},   {3.8, Cue::NOTE, 0x14},
      {3.8, Cue::NOTE, 0x30},     {4.2, Cue::SUCCESS, 0},     {5.8, Cue::REACTION, 0x34}, {5.8, Cue::REACTION, 0x14},
      {6.6, Cue::REACTION, 0x04}, {7.4, Cue::REACTION, 0x14}, {8.2, Cue::REACTION, 0x24}, {8.6, Cue::NOTE, 0x43},
      {9.0, Cue::NOTE, 0x20},     {9.0, Cue::REACTION, 0x04}, {9.5, Cue::NOTE, 0x42},   {9.5, Cue::REACTION, 0x04},
      {10.0, Cue::NOTE, 0x44},    {10.1, Cue::NOTE, 0x6A},    {10.4, Cue::REACTION, 0x44}, {10.5, Cue::NOTE, 0x54},
      {11.3, Cue::NOTE, 0x41},    {12.5, Cue::NOTE, 0x21},    {12.6, Cue::NOTE, 0x11},  {12.7, Cue::NOTE, 0x12},
      {12.8, Cue::NOTE, 0x13},    {12.8, Cue::NOTE, 0x30},    {13.2, Cue::SUCCESS, 0},  {13.25, Cue::REACTION, 0x23},
  };
  HatchAPetScore s;
  VolumeLimiter::Config cfg;
  cfg.peak_limit = VolumeLimiter::amp_safe_peak(12, 3300);  // board_pins.hpp: 12 dB strap, 3.3 V
  VolumeLimiter lim(cfg);
  lim.set_level(VolumeLimiter::kMaxLevel);
  BusyStats st;
  std::size_t next = 0;
  const std::size_t total = ms(16000);
  int16_t prev = 0;
  double sum = 0;
  int32_t raw[256];
  int16_t out[256];
  for (std::size_t t = 0; t < total; t += 256) {
    while (next < sizeof(seq) / sizeof(seq[0]) && ms(seq[next].s * 1000) <= t) {
      s.cue(seq[next].c, seq[next].v);
      ++next;
    }
    std::fill(raw, raw + 256, 0);
    s.render_add(raw, 256);
    st.min_comp_db = std::min(st.min_comp_db, static_cast<int>(std::floor(s.comp_reduction_db())));
    for (int32_t v : raw) {
      st.score_peak = std::max(st.score_peak, std::abs(v));
      st.over_ceiling += std::abs(v) > cfg.peak_limit ? 1 : 0;
      st.over_full += std::abs(v) > 32767 ? 1 : 0;
    }
    lim.process(raw, out, 256);
    st.min_limiter_q15 = std::min(st.min_limiter_q15, lim.limiter_gain_q15());
    for (int16_t v : out) {
      st.clamped += std::abs(static_cast<int>(v)) >= cfg.peak_limit ? 1 : 0;
      if (std::abs(static_cast<int32_t>(v) - prev) > st.max_step) { st.max_step = std::abs(static_cast<int32_t>(v) - prev); }
      prev = v;
      sum += v;
    }
  }
  st.dc = sum / static_cast<double>(total);
  return st;
}

void test_busy_sequence_headroom() {
  const BusyStats st = busy_run();
  std::printf("busy: score peak=%d (%.2f FS) >ceiling=%zu >FS=%zu comp GR=%d dB limiter min=%.2f clamped=%zu "
              "max step=%d dc=%.1f\n",
              st.score_peak, st.score_peak / 32767.0, st.over_ceiling, st.over_full, st.min_comp_db,
              st.min_limiter_q15 / 32768.0, st.clamped, st.max_step, st.dc);
  // Headroom: at full volume the busiest moments stay under the speaker
  // ceiling, so the limiter never has to pump and nothing is clamped.
  EXPECT(st.over_full == 0);
  EXPECT(st.over_ceiling == 0);
  EXPECT(st.min_limiter_q15 > VolumeLimiter::kUnityQ15 * 9 / 10);
  EXPECT(st.clamped == 0);
  EXPECT(std::fabs(st.dc) < 20);
}

void test_feedback_routes_hatch_cues() {
  AudioFeedback fb;
  fb.set_level(VolumeLimiter::kMaxLevel);
  std::vector<int16_t> out(ms(400));
  fb.render(out.data(), out.size());
  fb.set_scene(static_cast<uint8_t>(GameId::HATCH_A_PET));
  fb.play(Cue::AMBIENT, 0);
  fb.play(Cue::SUCCESS, 0);
  out.assign(ms(900), 0);
  fb.render(out.data(), out.size());
  int m = 0;
  for (int16_t v : out) m = std::max(m, std::abs(static_cast<int>(v)));
  EXPECT(m > 1000 && m <= VolumeLimiter::kPeakLimit);
  EXPECT(fb.cues_played() == 3);  // scene-change whoop + AMBIENT + SUCCESS
  fb.set_scene(IFeedback::kSceneMenu);
  out.assign(ms(1500), 0);
  fb.render(out.data(), out.size());
  out.assign(ms(200), 0);
  fb.render(out.data(), out.size());
  m = 0;
  for (int16_t v : out) m = std::max(m, std::abs(static_cast<int>(v)));
  EXPECT(m == 0);
}

}  // namespace

int main() {
  test_degree_frequencies_match_reference();
  test_silent_until_ambient();
  test_master_ramps_in();
  test_knock_is_a_woodblock_with_pitch_by_crack();
  test_knock_knock_later_is_delayed();
  test_chirp_glides_up();
  test_every_cue_sounds_and_differs();
  test_voice_limit_is_eight();
  test_hatch_pile_up_stays_bounded();
  test_lullaby_fades_in_and_stays_under_cues();
  test_lullaby_is_deterministic();
  test_lullaby_ducks_under_cues();
  test_lullaby_has_its_own_voices();
  test_stop_fades_without_click();
  test_render_cost();
  test_busy_sequence_headroom();
  test_feedback_routes_hatch_cues();
  return testing::finish("audio_hatch_a_pet_score_test");
}
