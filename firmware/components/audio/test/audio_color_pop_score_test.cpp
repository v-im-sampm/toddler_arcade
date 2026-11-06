// Host tests for ColorPopScore, the port of quality_bar.html's Web Audio score.
// Host only: checks pitch, timing, voice limit, ramp-in and loudness bounds;
// it does not prove acoustic level on the speaker.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/color_pop_score.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::audio;

namespace {

std::size_t ms(double v) { return static_cast<std::size_t>(v * kSampleRateHz / 1000.0); }

std::vector<int32_t> run(ColorPopScore& s, std::size_t n) {
  std::vector<int32_t> out(n, 0);
  s.render_add(out.data(), n);
  return out;
}

int32_t peak(const std::vector<int32_t>& v, std::size_t from = 0, std::size_t to = SIZE_MAX) {
  int32_t m = 0;
  for (std::size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs(v[i]));
  return m;
}

// Zero-crossing pitch estimate over [from, to).
double pitch(const std::vector<int32_t>& v, std::size_t from, std::size_t to) {
  int crossings = 0;
  for (std::size_t i = from + 1; i < to; ++i) crossings += (v[i - 1] < 0) != (v[i] < 0) ? 1 : 0;
  return crossings * 0.5 * kSampleRateHz / static_cast<double>(to - from);
}

void test_degree_frequencies_match_reference() {
  // degFreq(): 523.25 * 2^((PENT[i] + 12 * (o + oct)) / 12)
  EXPECT(std::fabs(ColorPopScore::degree_hz(0, 0) - 523.25f) < 0.01f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(0, -1) - 261.625f) < 0.01f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(4, 0) - 880.0f) < 0.1f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(5, 0) - 1046.5f) < 0.1f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(7, 0) - 1318.5f) < 0.2f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(10, 0) - 2093.0f) < 0.2f);
  EXPECT(std::fabs(ColorPopScore::degree_hz(2, 1) - 2 * 659.26f) < 0.2f);
  EXPECT(ColorPopScore::color_degree(0) == 0 && ColorPopScore::color_degree(1) == 4);
  EXPECT(ColorPopScore::color_degree(2) == 2 && ColorPopScore::color_degree(3) == 1);
}

void test_silent_until_ambient() {
  ColorPopScore s;
  EXPECT(peak(run(s, 4000)) == 0);
  EXPECT(!s.running());
  EXPECT(!s.cue(Cue::TAP, 0));        // not owned: AudioFeedback falls back
  EXPECT(!s.cue(Cue::TRY_AGAIN, 0));
}

void test_master_ramps_in() {
  // setTargetAtTime(0.35, now, 0.1): the first prompt note right after the
  // start is much quieter than the same note 0.5 s later.
  ColorPopScore a;
  a.cue(Cue::AMBIENT, 0);
  a.cue(Cue::REACTION, 0);
  const int32_t early = peak(run(a, ms(150)));
  ColorPopScore b;
  b.cue(Cue::AMBIENT, 0);
  run(b, ms(500));
  b.cue(Cue::REACTION, 0);
  const int32_t late = peak(run(b, ms(150)));
  EXPECT(early > 0 && late > early * 2);
}

void test_success_note_pitch_and_timing() {
  ColorPopScore s;
  s.cue(Cue::AMBIENT, 0);
  run(s, ms(1000));
  s.cue(Cue::SUCCESS, 1);  // BLUE: plop now, marimba(degFreq(4)) at +0.03 s
  const auto out = run(s, ms(700));
  EXPECT(peak(out, 0, ms(5)) > 1000);  // plop starts immediately
  // After the plop and overtone (0.09 s) the 880 Hz marimba body remains.
  const double hz = pitch(out, ms(100), ms(220));
  EXPECT(std::fabs(hz - 880.0) < 25.0);
  // Exponential decay to 0.0001 at 0.55 s: much quieter late than early.
  EXPECT(peak(out, ms(500), ms(560)) * 8 < peak(out, ms(40), ms(100)));
  EXPECT(peak(out, ms(640), ms(700)) < 1500);  // stopped (dur + 0.05): only the wave bed left
}

void test_motifs_are_distinct_and_timed() {
  // Each colour's motif has its own notes; yellow has four onsets.
  std::vector<int32_t> sounds[4];
  for (uint8_t c = 0; c < 4; ++c) {
    ColorPopScore s;
    s.cue(Cue::AMBIENT, 0);
    run(s, ms(800));
    s.cue(Cue::PROMPT, c);
    sounds[c] = run(s, ms(900));
    EXPECT(peak(sounds[c]) > 2000);
  }
  // RED: two C4 notes 0.2 s apart (degree 0, octave -1).
  EXPECT(std::fabs(pitch(sounds[0], ms(60), ms(190)) - 261.6) < 15.0);
  for (int a = 0; a < 4; ++a) {
    for (int b = a + 1; b < 4; ++b) EXPECT(sounds[a] != sounds[b]);
  }
}

void test_voice_limit_is_eight() {
  ColorPopScore s;
  s.cue(Cue::AMBIENT, 0);
  run(s, 16);
  s.cue(Cue::CELEBRATE, 1);  // 8 marimba notes
  EXPECT(s.notes() == 8);
  s.cue(Cue::REACTION, 0);   // the 9th is refused, like voiceOk()
  EXPECT(s.notes() == 8);
  run(s, ms(1600));           // every note has ended
  EXPECT(s.notes() == 0);
  s.cue(Cue::REACTION, 0);
  EXPECT(s.notes() == 1);
}

void test_celebration_stays_bounded() {
  // Loudest pile-up: big celebration + success + sparkles. The compressor and
  // trim keep the pre-limiter mix within the range VolumeLimiter handles.
  ColorPopScore s;
  s.cue(Cue::AMBIENT, 0);
  run(s, ms(800));
  s.cue(Cue::SUCCESS, 3);
  s.cue(Cue::CELEBRATE, 1);
  s.cue(Cue::NOTE, 0x34);
  const auto out = run(s, ms(1500));
  EXPECT(peak(out) > 8000);
  EXPECT(peak(out) < 32767 * 2);
}

void test_wave_bed_is_quiet_and_continuous() {
  ColorPopScore s;
  s.cue(Cue::AMBIENT, 0);
  run(s, ms(1000));
  const auto bed = run(s, ms(2000));
  EXPECT(peak(bed) > 0);
  EXPECT(peak(bed) < 1500);  // gain 0.018..0.032 behind a 420 Hz lowpass
}

void test_stop_fades_without_click() {
  ColorPopScore s;
  s.cue(Cue::AMBIENT, 0);
  run(s, ms(500));
  s.cue(Cue::PROMPT, 0);
  const auto before = run(s, ms(100));
  s.stop();
  const auto fade = run(s, ms(300));
  EXPECT(std::abs(fade[0]) <= peak(before));  // no jump on stop
  EXPECT(peak(fade, ms(200)) == 0);
  EXPECT(!s.running() && s.notes() == 0);
}

void test_feedback_routes_color_pop_cues() {
  AudioFeedback fb;
  fb.set_level(VolumeLimiter::kMaxLevel);
  std::vector<int16_t> out(ms(400));
  fb.render(out.data(), out.size());
  fb.set_scene(static_cast<uint8_t>(GameId::COLOR_POP));
  fb.play(Cue::AMBIENT, 0);
  fb.play(Cue::SUCCESS, 2);
  out.assign(ms(600), 0);
  fb.render(out.data(), out.size());
  int m = 0;
  for (int16_t v : out) m = std::max(m, std::abs(static_cast<int>(v)));
  EXPECT(m > 1000 && m <= VolumeLimiter::kPeakLimit);
  EXPECT(fb.cues_played() == 3);  // scene-change whoop + AMBIENT + SUCCESS
  // Leaving the game fades the score out (including the wave bed).
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
  test_success_note_pitch_and_timing();
  test_motifs_are_distinct_and_timed();
  test_voice_limit_is_eight();
  test_celebration_stays_bounded();
  test_wave_bed_is_quiet_and_continuous();
  test_stop_fades_without_click();
  test_feedback_routes_color_pop_cues();
  return testing::finish("audio_color_pop_score_test");
}
