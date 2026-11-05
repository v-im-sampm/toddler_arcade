#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/sampler.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::audio;

namespace {

struct Rig {
  std::vector<int16_t> clip = std::vector<int16_t>(Sampler::kMaxSamples, 7);
  std::vector<int16_t> record = std::vector<int16_t>(Sampler::kMaxSamples, 7);
  std::vector<int16_t> echo = std::vector<int16_t>(Sampler::kEchoSamples, 7);
  Sampler s{clip.data(), record.data(), echo.data()};

  Rig() { s.set_mic_hardware(true); }

  std::vector<int16_t> mix(std::size_t n) {
    std::vector<int16_t> out(n, 0);
    s.mix(out.data(), n);
    return out;
  }
  // Renders one sample at a time until the voices go silent.
  std::vector<int16_t> play_to_end(uint32_t rate, std::size_t limit = 200000) {
    s.play(rate, false);
    std::vector<int16_t> out;
    int16_t v = 0;
    s.mix(&v, 1);
    out.push_back(v);
    while (s.playhead_permille() >= 0 && out.size() < limit) {
      v = 0;
      s.mix(&v, 1);
      out.push_back(v);
    }
    return out;
  }
  ISampler::RecordResult record_fixture(const std::vector<int16_t>& pcm) {
    s.start_recording();
    mix(16);
    for (std::size_t i = 0; i < pcm.size(); i += 256) {
      const std::size_t n = pcm.size() - i < 256 ? pcm.size() - i : 256;
      s.capture(pcm.data() + i, n);
    }
    s.stop_recording();
    mix(16);
    return s.take_record_result();
  }
};

std::vector<int16_t> silence(std::size_t n) { return std::vector<int16_t>(n, 0); }

void append_tone(std::vector<int16_t>& v, std::size_t n, double hz, double amp) {
  for (std::size_t i = 0; i < n; ++i) {
    v.push_back(static_cast<int16_t>(std::lround(amp * std::sin(6.283185307179586 * hz * static_cast<double>(i) / 16000.0))));
  }
}

int32_t peak(const std::vector<int16_t>& v, std::size_t from = 0, std::size_t to = SIZE_MAX) {
  int32_t p = 0;
  for (std::size_t i = from; i < v.size() && i < to; ++i) p = std::max(p, static_cast<int32_t>(std::abs(v[i])));
  return p;
}

bool all_zero(const std::vector<int16_t>& v) {
  for (int16_t s : v)
    if (s != 0) return false;
  return true;
}

std::vector<int16_t> voice_fixture() {
  std::vector<int16_t> pcm = silence(4800);  // 300 ms
  append_tone(pcm, 8000, 440.0, 6000.0);      // 500 ms
  const auto tail = silence(6400);            // 400 ms
  pcm.insert(pcm.end(), tail.begin(), tail.end());
  return pcm;
}

void test_constructor_zeroes_buffers() {
  Rig r;
  EXPECT(all_zero(r.clip) && all_zero(r.record) && all_zero(r.echo));
  EXPECT(r.s.clip_samples() == 0);
  EXPECT(r.s.playhead_permille() == -1);
}

void test_builtins_render_and_normalize() {
  for (uint8_t i = 0; i < ISampler::kBuiltinCount; ++i) {
    Rig r;
    const uint32_t gen = r.s.clip_generation();
    r.s.load_builtin(i);
    r.mix(64);
    EXPECT(r.s.clip_generation() != gen);
    EXPECT(r.s.clip_samples() > 3000 && r.s.clip_samples() < 16000);
    const int32_t p = peak(r.clip);
    EXPECT(p >= Sampler::kTargetPeak - 64 && p <= Sampler::kTargetPeak);
    int max_thumb = 0;
    for (std::size_t c = 0; c < ISampler::kThumbColumns; ++c) max_thumb = std::max(max_thumb, static_cast<int>(r.s.thumb(c)));
    EXPECT(max_thumb > 240);
    EXPECT(r.clip[0] == 0);  // fade-in
  }
}

void test_trim_and_normalize_recording() {
  Rig r;
  EXPECT(r.record_fixture(voice_fixture()) == ISampler::RecordResult::ACCEPTED);
  const uint32_t len = r.s.clip_samples();
  // 500 ms of sound + 20 ms pre-roll + 80 ms tail, give or take the threshold.
  EXPECT(len >= 9400 && len <= 9700);
  const int32_t p = peak(r.clip);
  EXPECT(p >= Sampler::kTargetPeak - 600 && p <= Sampler::kTargetPeak);
  EXPECT(all_zero(r.record));  // the raw take is gone
  for (uint32_t i = len; i < Sampler::kMaxSamples; ++i) {
    if (r.clip[i] != 0) {
      EXPECT(r.clip[i] == 0);
      break;
    }
  }
}

void test_too_quiet_and_too_short_keep_previous_sound() {
  Rig r;
  r.s.load_builtin(0);
  r.mix(16);
  const uint32_t gen = r.s.clip_generation();
  const uint32_t len = r.s.clip_samples();
  const std::vector<int16_t> before = r.clip;

  std::vector<int16_t> quiet;
  append_tone(quiet, 16000, 300.0, 200.0);
  EXPECT(r.record_fixture(quiet) == ISampler::RecordResult::TOO_QUIET);
  std::vector<int16_t> blip;
  append_tone(blip, 800, 300.0, 9000.0);  // 50 ms
  EXPECT(r.record_fixture(blip) == ISampler::RecordResult::TOO_QUIET);

  EXPECT(r.s.clip_generation() == gen && r.s.clip_samples() == len);
  EXPECT(r.clip == before);
  EXPECT(all_zero(r.record));
}

void test_record_cap_stops_at_three_seconds() {
  Rig r;
  r.s.start_recording();
  r.mix(16);
  std::vector<int16_t> loud;
  append_tone(loud, 52000, 500.0, 8000.0);
  for (std::size_t i = 0; i < loud.size(); i += 256) r.s.capture(loud.data() + i, 256);
  EXPECT(r.s.take_record_result() == ISampler::RecordResult::ACCEPTED);
  EXPECT(!r.s.recording());
  EXPECT(r.s.clip_samples() <= Sampler::kMaxSamples && r.s.clip_samples() > 40000);
}

void test_recording_silences_output_and_needs_mic() {
  Rig r;
  r.s.load_builtin(1);
  r.mix(16);
  r.s.start_recording();
  std::vector<int16_t> out(256, 1234);
  r.s.mix(out.data(), out.size());
  EXPECT(r.s.recording());
  EXPECT(all_zero(out));
  std::vector<int16_t> pcm;
  append_tone(pcm, 1000, 300.0, 5000.0);
  r.s.capture(pcm.data(), pcm.size());
  EXPECT(r.s.record_progress_permille() > 0);
  EXPECT(r.s.input_level() > 0);
  r.s.cancel_recording();
  r.mix(16);
  EXPECT(!r.s.recording() && all_zero(r.record));
  EXPECT(r.s.take_record_result() == ISampler::RecordResult::NONE);

  Rig off;
  off.s.set_mic_allowed(false);
  EXPECT(!off.s.mic_available());
  off.s.start_recording();
  off.mix(16);
  EXPECT(!off.s.recording());
  Rig no_hw;
  no_hw.s.set_mic_hardware(false);
  EXPECT(!no_hw.s.mic_available());
}

void test_pitch_changes_length() {
  Rig r;
  EXPECT(r.record_fixture(voice_fixture()) == ISampler::RecordResult::ACCEPTED);
  const double len = r.s.clip_samples();
  const double unity = static_cast<double>(r.play_to_end(ISampler::kRateUnity).size());
  const double low = static_cast<double>(r.play_to_end(ISampler::kRateMin).size());
  const double high = static_cast<double>(r.play_to_end(ISampler::kRateMax).size());
  EXPECT(std::fabs(unity - len) < 4);
  EXPECT(std::fabs(low - 2 * len) < 8);   // 0.5x doubles the length
  EXPECT(std::fabs(high - len / 2) < 4);  // 2x halves it
}

void test_reverse_is_exact_mirror() {
  Rig r;
  r.s.load_builtin(0);
  r.mix(16);
  const std::size_t len = r.s.clip_samples();
  const auto fwd = r.play_to_end(ISampler::kRateUnity);
  r.s.set_effects(ISampler::kFxReverse);
  const auto rev = r.play_to_end(ISampler::kRateUnity);
  EXPECT(fwd.size() == rev.size());
  bool mirror = true;
  // Skip the 3 ms attack at the start of each pass.
  for (std::size_t i = Sampler::kAttackSamples + 1; i + Sampler::kAttackSamples + 1 < len; ++i) {
    if (rev[i] != fwd[len - 1 - i]) mirror = false;
  }
  EXPECT(mirror);
}

void test_echo_tail_is_bounded_and_decays() {
  Rig r;
  r.s.load_builtin(2);
  r.mix(16);
  r.s.set_effects(ISampler::kFxEcho);
  r.s.play(ISampler::kRateUnity, false);
  const std::size_t len = r.s.clip_samples();
  const auto out = r.mix(16000 * 3);
  EXPECT(peak(out, len + 100, len + 4000) > 1000);  // an echo is heard after the sound ends
  EXPECT(peak(out, 16000 * 2) < 64);                // and it fades away
  r.s.set_effects(0);
  r.mix(16);
  EXPECT(r.s.playhead_permille() == -1);
}

void test_robot_and_wobble_change_the_sound() {
  Rig r;
  r.s.load_builtin(0);
  r.mix(16);
  r.s.play(ISampler::kRateUnity, false);
  const auto plain = r.mix(4000);
  r.s.stop_all();
  r.mix(2000);
  r.s.set_effects(ISampler::kFxRobot);
  r.s.play(ISampler::kRateUnity, false);
  const auto robot = r.mix(4000);
  EXPECT(plain != robot);
  EXPECT(peak(robot) > 1000);
  r.s.set_effects(ISampler::kFxWobble);
  const double wob = static_cast<double>(r.play_to_end(ISampler::kRateUnity).size());
  EXPECT(std::fabs(wob - static_cast<double>(r.s.clip_samples())) < r.s.clip_samples() * 0.1);
}

void test_loop_bend_and_release() {
  Rig r;
  r.s.load_builtin(1);
  r.mix(16);
  const std::size_t len = r.s.clip_samples();
  r.s.play(ISampler::kRateUnity, true);
  r.mix(len * 2);  // still going past two lengths
  EXPECT(r.s.playhead_permille() >= 0);
  r.s.bend(ISampler::kRateMax);
  r.mix(512);
  EXPECT(r.s.playhead_permille() >= 0);
  r.s.release();
  r.mix(Sampler::kReleaseSamples + 16);
  EXPECT(r.s.playhead_permille() == -1);
  // A short tap: the loop is ended and the pass plays to its natural end.
  r.s.play(ISampler::kRateUnity, true);
  r.mix(100);
  r.s.end_loop();
  r.mix(len / 2);
  EXPECT(r.s.playhead_permille() >= 0);
  r.mix(len);
  EXPECT(r.s.playhead_permille() == -1);
}

void test_wipe_zeroes_everything_and_drops_stale_commands() {
  Rig r;
  r.record_fixture(voice_fixture());
  r.s.set_effects(ISampler::kFxEcho);
  r.s.play(ISampler::kRateUnity, true);
  r.mix(3000);
  r.s.wipe();
  r.mix(16);
  EXPECT(all_zero(r.clip) && all_zero(r.record) && all_zero(r.echo));
  EXPECT(r.s.clip_samples() == 0 && r.s.playhead_permille() == -1);
  for (std::size_t c = 0; c < ISampler::kThumbColumns; ++c) EXPECT(r.s.thumb(c) == 0);
  EXPECT(peak(r.mix(1000)) == 0);

  // A recording queued before a wipe never starts...
  r.s.start_recording();
  r.s.wipe();
  r.mix(16);
  EXPECT(!r.s.recording());
  // ...and wipe mid-recording discards the take.
  r.s.start_recording();
  r.mix(16);
  std::vector<int16_t> pcm;
  append_tone(pcm, 4000, 300.0, 5000.0);
  r.s.capture(pcm.data(), pcm.size());
  r.s.wipe();
  r.mix(16);
  EXPECT(!r.s.recording() && all_zero(r.record) && r.s.clip_samples() == 0);
  // Commands after a wipe still run.
  r.s.wipe();
  r.s.load_builtin(0);
  r.mix(16);
  EXPECT(r.s.clip_samples() > 0);
}

void test_full_output_stays_under_speaker_limit() {
  std::vector<int16_t> clip(Sampler::kMaxSamples), record(Sampler::kMaxSamples), echo(Sampler::kEchoSamples);
  Sampler s(clip.data(), record.data(), echo.data());
  AudioFeedback fb;
  fb.attach_sampler(&s);
  fb.set_level(VolumeLimiter::kMaxLevel);
  fb.set_parent_cap(VolumeLimiter::kMaxLevel);
  s.load_builtin(2);
  s.set_effects(ISampler::kFxAll);
  s.play(ISampler::kRateMax, true);
  s.play(ISampler::kRateUnity, false);
  fb.play(Cue::CELEBRATE, 0);
  std::vector<int16_t> out(256);
  int32_t worst = 0;
  for (int b = 0; b < 200; ++b) {
    fb.render(out.data(), out.size());
    worst = std::max(worst, peak(out));
  }
  EXPECT(worst > 0);
  EXPECT(worst <= VolumeLimiter::kPeakLimit);
}

}  // namespace

int main() {
  test_constructor_zeroes_buffers();
  test_builtins_render_and_normalize();
  test_trim_and_normalize_recording();
  test_too_quiet_and_too_short_keep_previous_sound();
  test_record_cap_stops_at_three_seconds();
  test_recording_silences_output_and_needs_mic();
  test_pitch_changes_length();
  test_reverse_is_exact_mirror();
  test_echo_tail_is_bounded_and_decays();
  test_robot_and_wobble_change_the_sound();
  test_loop_bend_and_release();
  test_wipe_zeroes_everything_and_drops_stale_commands();
  test_full_output_stays_under_speaker_limit();
  return testing::finish("audio_sampler_test");
}
