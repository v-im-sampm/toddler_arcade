#include <cstdint>
#include <initializer_list>

#include "little_arcade/games/beat_sharks.hpp"
#include "little_arcade/rng.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::beat_sharks;

namespace {

using G = BeatSharksGame;

// Records what the game asks of the sampler; the test plays the audio task.
class FakeSampler final : public ISampler {
 public:
  bool mic = true;
  bool recording = false;
  RecordResult pending = RecordResult::NONE;
  uint16_t progress = 0;
  int wipes = 0, starts = 0, stops = 0, cancels = 0, plays = 0, loops = 0, stop_alls = 0, builtins = 0;
  uint8_t last_builtin = 0xFF;
  uint8_t fx = 0xFF;
  uint32_t last_rate = 0;

  bool mic_available() const override { return mic; }
  void start_recording() override {
    ++starts;
    recording = true;
  }
  void stop_recording() override { ++stops; }
  void cancel_recording() override {
    ++cancels;
    recording = false;
  }
  RecordResult take_record_result() override {
    const RecordResult r = pending;
    pending = RecordResult::NONE;
    if (r != RecordResult::NONE) recording = false;
    return r;
  }
  uint16_t record_progress_permille() const override { return progress; }
  uint8_t input_level() const override { return 0; }
  void play(uint32_t rate, bool loop) override {
    ++plays;
    loops += loop ? 1 : 0;
    last_rate = rate;
  }
  void bend(uint32_t) override {}
  void end_loop() override {}
  void release() override {}
  void stop_all() override { ++stop_alls; }
  void set_effects(uint8_t mask) override { fx = mask; }
  void load_builtin(uint8_t i) override {
    ++builtins;
    last_builtin = i;
  }
  void wipe() override {
    ++wipes;
    recording = false;
  }
  uint32_t clip_generation() const override { return 0; }
  uint32_t clip_samples() const override { return 8000; }
  uint8_t thumb(std::size_t) const override { return 0; }
  int16_t playhead_permille() const override { return -1; }
};

struct Rig {
  Rng rng{7};
  testing::RecordingFeedback fb;
  GameContext ctx{rng, fb};
  FakeSampler sampler;
  G game;
  explicit Rig(bool with_sampler = true) : game(with_sampler ? &sampler : nullptr) { game.enter(ctx); }
  void tap(const Rect& r) { game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), 0)); }
  void tap_cell(std::size_t row, std::size_t step) { tap(G::cell_rect(row, step)); }
  void say(Intent i) { game.handle(make_voice_event(i, 0)); }
  // Skips the intro quietly and stops the loop so a test starts from silence.
  void stopped() {
    game.update(G::kIntroMs);
    tap(G::kPlayButton);
    fb.clear();
  }
  int notes(uint8_t variant) const {
    int n = 0;
    for (int i = 0; i < fb.count && i < testing::RecordingFeedback::kCapacity; ++i)
      n += fb.entries[i].cue == Cue::NOTE && fb.entries[i].variant == variant ? 1 : 0;
    return n;
  }
};

void test_layout_is_toddler_safe() {
  Rect all[3 + kRows * kSteps];
  std::size_t n = 0;
  all[n++] = G::kPlayButton;
  all[n++] = G::kRecordButton;
  all[n++] = G::kTempoButton;
  for (std::size_t r = 0; r < kRows; ++r)
    for (std::size_t s = 0; s < kSteps; ++s) all[n++] = G::cell_rect(r, s);
  for (std::size_t i = 0; i < n; ++i) {
    EXPECT(all[i].on_screen());
    EXPECT(all[i].large_enough());
    EXPECT(!all[i].overlaps(kHomeButton));
    EXPECT(!all[i].overlaps(kMicButton));
    for (std::size_t j = i + 1; j < n; ++j) EXPECT(!all[i].overlaps(all[j]));
  }
  for (std::size_t i = 1; i < kSoundsPerRow; ++i) EXPECT(G::kVoiceRates[i] > G::kVoiceRates[i - 1]);
  EXPECT(G::kVoiceRates[1] == ISampler::kRateUnity);
  EXPECT(G::kVoiceRates[0] >= ISampler::kRateMin && G::kVoiceRates[2] <= ISampler::kRateMax);
}

void test_enter_has_starter_beat_and_clean_sampler() {
  Rig r;
  EXPECT(r.sampler.wipes == 1 && r.sampler.fx == 0 && r.sampler.last_builtin == 0);
  for (std::size_t row = 0; row < kRows; ++row)
    for (std::size_t s = 0; s < kSteps; ++s) {
      const bool starter = row == 2 && (s == 0 || s == 3);
      EXPECT(r.game.cell(row, s) == (starter ? 1 : 0));
    }
  EXPECT(r.game.phase() == Phase::INTRO && !r.game.playing());
  EXPECT(r.fb.count == 0);  // quiet entry
  r.game.update(G::kIntroMs);
  EXPECT(r.game.phase() == Phase::PLAY && r.game.playing() && r.game.step() == 0);
  EXPECT(r.fb.count_of(Cue::PROMPT) == 1);
  EXPECT(r.notes(6) == 1);  // big BOOM on step 1
}

void test_tap_cycles_three_sounds_then_empty() {
  Rig r;
  r.stopped();
  for (std::size_t row = 0; row < kRows; ++row) {
    const std::size_t s = 1;
    for (uint8_t want = 1; want <= kSoundsPerRow; ++want) {
      r.fb.clear();
      const int plays = r.sampler.plays;
      r.tap_cell(row, s);
      EXPECT(r.game.cell(row, s) == want);
      EXPECT(r.game.hopping(row, s));
      if (row == 0) {
        EXPECT(r.sampler.plays == plays + 1 && r.sampler.last_rate == G::kVoiceRates[want - 1]);
        EXPECT(r.sampler.loops == 0);
        EXPECT(r.fb.count == 0);
      } else {
        EXPECT(r.fb.count == 1 && r.notes(static_cast<uint8_t>(row * kSoundsPerRow + want - 1)) == 1);
      }
    }
    r.fb.clear();
    const int plays = r.sampler.plays;
    r.tap_cell(row, s);
    EXPECT(r.game.cell(row, s) == 0);  // fourth tap: back to empty water
    EXPECT(!r.game.hopping(row, s));
    EXPECT(r.fb.count == 0 && r.sampler.plays == plays);  // and no sound
  }
}

void test_voice_row_is_mono() {
  Rig r;
  r.stopped();
  const int before = r.sampler.stop_alls;
  r.tap_cell(0, 2);
  r.tap_cell(0, 2);
  EXPECT(r.sampler.stop_alls == before + 2);
  EXPECT(r.sampler.plays == 2);
}

void test_no_sampler_voice_row_uses_synth_fallback() {
  Rig r(false);
  r.stopped();
  EXPECT(r.game.sparkle_mode());
  r.tap_cell(0, 0);
  r.tap_cell(0, 0);
  r.tap_cell(0, 0);
  EXPECT(r.notes(0) == 1 && r.notes(1) == 1 && r.notes(2) == 1);
  // Sparkle with no sampler still previews the voice row and places sharks.
  r.tap_cell(0, 0);  // back to empty
  r.fb.clear();
  r.tap(G::kRecordButton);
  EXPECT(r.game.phase() != Phase::RECORDING);
  EXPECT(r.game.cell(0, 0) == 2 && r.game.cell(0, 3) == 2);
  EXPECT(r.notes(1) == 1);
}

void test_step_clock_has_no_drift() {
  Rig r;
  r.game.update(G::kIntroMs);  // step 1 sounds now
  const uint32_t ms = G::kStepMs[G::kDefaultTempo];
  // 60 s of uneven frames (13, 17, 16, 21 ms...).
  static constexpr uint32_t kFrames[] = {13, 17, 16, 21, 9, 24};
  uint32_t total = 0;
  for (uint32_t i = 0; total < 60000; ++i) {
    const uint32_t dt = kFrames[i % 6];
    r.game.update(dt);
    total += dt;
  }
  EXPECT(r.game.steps_played() == 1 + total / ms);
  EXPECT(r.game.step() == static_cast<int>((total / ms) % kSteps));
  EXPECT(r.game.step_progress_permille() == (total % ms) * 1000 / ms);
}

void test_loop_plays_each_column_in_order() {
  Rig r;
  r.stopped();
  r.tap_cell(1, 2);  // clap on step 3
  r.tap_cell(2, 3);  // step 4 BOOM -> tom
  r.fb.clear();
  r.tap(G::kPlayButton);
  EXPECT(r.game.playing() && r.game.step() == 0 && r.notes(6) == 1);
  const uint32_t ms = G::kStepMs[G::kDefaultTempo];
  r.game.update(ms);
  EXPECT(r.game.step() == 1 && r.fb.count == 1);
  r.game.update(ms);
  EXPECT(r.game.step() == 2 && r.notes(3) == 1 && r.game.hopping(1, 2));
  r.game.update(ms);
  EXPECT(r.game.step() == 3 && r.notes(7) == 1);
  r.game.update(ms * 3);  // a long stall: one catch-up step, no pile of sounds
  EXPECT(r.game.step() == 0 && r.notes(6) == 2);
  EXPECT(r.fb.count == 4);
}

void test_stop_parks_and_play_restarts() {
  Rig r;
  r.game.update(G::kIntroMs);
  r.game.update(G::kStepMs[1] + 10);
  EXPECT(r.game.step() == 1);
  r.tap(G::kPlayButton);
  EXPECT(!r.game.playing());
  r.fb.clear();
  r.game.update(5000);
  EXPECT(r.fb.count == 0 && r.game.step() == 1);  // parked, silent
  r.tap(G::kPlayButton);
  EXPECT(r.game.playing() && r.game.step() == 0 && r.notes(6) == 1);
}

void test_tempo_changes_next_step_without_restart() {
  Rig r;
  r.game.update(G::kIntroMs);
  r.game.update(100);
  r.fb.clear();
  r.tap(G::kTempoButton);
  EXPECT(r.game.tempo() == 2 && r.game.step() == 0 && r.game.playing());
  EXPECT(r.fb.count_of(Cue::TAP) == 1);
  r.game.update(G::kStepMs[2] - 100);  // the rest of a rabbit step
  EXPECT(r.game.step() == 1);
  r.tap(G::kTempoButton);
  r.tap(G::kTempoButton);
  EXPECT(r.game.tempo() == 1);  // rabbit -> turtle -> walk
}

void test_preview_skipped_when_fin_is_arriving() {
  Rig r;
  r.game.update(G::kIntroMs);
  const uint32_t ms = G::kStepMs[G::kDefaultTempo];
  r.game.update(ms - 30);  // fin 30 ms before step 2
  r.fb.clear();
  r.tap_cell(1, 1);
  EXPECT(r.game.cell(1, 1) == 1 && r.fb.count == 0);
  r.game.update(30);
  EXPECT(r.notes(3) == 1);  // heard once, on the beat
  // Further away, the preview plays.
  r.fb.clear();
  r.tap_cell(1, 3);
  EXPECT(r.notes(3) == 1);
}

void test_full_ocean_celebrates_once_and_rearms() {
  Rig r;
  r.stopped();
  for (std::size_t s : {1u, 2u, 4u}) r.tap_cell(1, s);
  EXPECT(r.game.phase() == Phase::PLAY && r.fb.count_of(Cue::CELEBRATE) == 0);
  r.tap_cell(1, 5);
  EXPECT(r.game.full_ocean());
  EXPECT(r.game.phase() == Phase::CELEBRATE && r.game.celebration() == Celebration::FULL_OCEAN);
  EXPECT(r.fb.count_of(Cue::CELEBRATE) == 1);
  r.tap_cell(0, 5);  // still full: no second party; taps keep working mid-celebration
  EXPECT(r.game.cell(0, 5) == 1 && r.fb.count_of(Cue::CELEBRATE) == 1);
  r.game.update(G::kCelebrateMs);
  EXPECT(r.game.phase() == Phase::PLAY);
  // Empty column 2 (clap -> 2 -> 3 -> empty), then fill it again.
  r.tap_cell(1, 1);
  r.tap_cell(1, 1);
  r.tap_cell(1, 1);
  EXPECT(!r.game.full_ocean());
  r.tap_cell(2, 1);
  EXPECT(r.fb.count_of(Cue::CELEBRATE) == 2);
}

void test_recording_pauses_loop_autostops_and_places_sharks() {
  Rig r;
  r.game.update(G::kIntroMs);
  r.game.update(G::kStepMs[1] + 20);
  EXPECT(r.game.step() == 1 && r.game.playing());
  const int stop_alls = r.sampler.stop_alls;
  r.tap(G::kRecordButton);
  EXPECT(r.game.phase() == Phase::RECORDING && r.sampler.starts == 1);
  EXPECT(!r.game.playing() && r.sampler.stop_alls == stop_alls + 1);
  r.fb.clear();
  r.game.update(2000);
  EXPECT(r.fb.count == 0 && r.game.step() == 1);  // fin holds still, no sound
  // Cells and voice are ignored while listening.
  r.tap_cell(1, 0);
  r.say(Intent::AGAIN);
  EXPECT(r.game.cell(1, 0) == 0 && !r.game.playing());
  r.sampler.progress = G::kRecordStopPermille;
  r.game.update(16);
  EXPECT(r.sampler.stops == 1);
  r.game.update(16);
  EXPECT(r.sampler.stops == 1);  // only once
  r.sampler.pending = ISampler::RecordResult::ACCEPTED;
  r.game.update(16);
  EXPECT(r.game.phase() == Phase::CELEBRATE && r.game.celebration() == Celebration::NEW_SOUND);
  EXPECT(r.game.sounds_caught() == 1);
  EXPECT(r.game.cell(0, 0) == 2 && r.game.cell(0, 3) == 2);
  EXPECT(r.game.playing() && r.game.step() == 1);  // the loop picks up where it was
  r.game.update(G::kCelebrateMs);
  EXPECT(r.game.phase() == Phase::PLAY);
}

void test_recording_keeps_existing_voice_sharks() {
  Rig r;
  r.stopped();
  r.tap_cell(0, 5);
  r.tap_cell(0, 5);
  r.tap_cell(0, 5);  // small shark
  const int plays = r.sampler.plays;
  r.tap(G::kRecordButton);
  r.tap(G::kRecordButton);  // child stops early
  EXPECT(r.sampler.stops == 1);
  r.sampler.pending = ISampler::RecordResult::ACCEPTED;
  r.game.update(16);
  EXPECT(r.game.cell(0, 0) == 0 && r.game.cell(0, 5) == 3);
  EXPECT(!r.game.playing());
  EXPECT(r.sampler.plays == plays + 1 && r.sampler.last_rate == ISampler::kRateUnity);  // hear it once
}

void test_too_quiet_wiggles_without_failure_sound() {
  Rig r;
  r.game.update(G::kIntroMs);
  r.tap(G::kRecordButton);
  r.fb.clear();
  r.sampler.pending = ISampler::RecordResult::TOO_QUIET;
  r.game.update(16);
  EXPECT(r.game.phase() == Phase::PLAY && r.game.wiggling());
  EXPECT(r.game.playing());  // loop resumes
  EXPECT(r.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(r.game.cell(0, 0) == 0);
  r.game.update(G::kWiggleMs);
  EXPECT(!r.game.wiggling());
}

void test_recording_timeout_backstop() {
  Rig r;
  r.stopped();
  r.tap(G::kRecordButton);
  r.game.update(G::kRecordTimeoutMs);
  EXPECT(r.sampler.cancels == 1 && r.game.phase() == Phase::PLAY && r.game.wiggling());
}

void test_mic_off_sparkle_cycles_builtins() {
  Rig r;
  r.sampler.mic = false;
  r.stopped();
  EXPECT(r.game.sparkle_mode());
  for (uint8_t i = 1; i <= ISampler::kBuiltinCount; ++i) {
    r.tap(G::kRecordButton);
    EXPECT(r.game.phase() == Phase::PLAY && r.sampler.starts == 0);
    EXPECT(r.game.builtin_index() == i % ISampler::kBuiltinCount);
    EXPECT(r.sampler.last_builtin == i % ISampler::kBuiltinCount);
    EXPECT(r.sampler.last_rate == ISampler::kRateUnity && r.game.record_lit());
  }
  EXPECT(r.game.cell(0, 0) == 2 && r.game.cell(0, 3) == 2);
}

void test_voice_intents() {
  Rig r;
  r.say(Intent::HELP);  // skips the intro and starts the beat, no prompt over it
  EXPECT(r.game.phase() == Phase::PLAY && r.game.playing());
  EXPECT(r.fb.count_of(Cue::PROMPT) == 1);
  r.game.update(G::kStepMs[1] * 2 + 5);
  EXPECT(r.game.step() == 2);
  r.say(Intent::AGAIN);
  EXPECT(r.game.step() == 0 && r.game.playing());
  r.tap(G::kPlayButton);
  r.say(Intent::AGAIN);
  EXPECT(r.game.playing());
}

void test_intro_tap_on_play_does_not_stop() {
  Rig r;
  r.tap(G::kPlayButton);
  EXPECT(r.game.phase() == Phase::PLAY && r.game.playing());
  EXPECT(r.fb.count_of(Cue::PROMPT) == 0);
  // A cell tap during the intro starts the beat and still edits.
  Rig q;
  q.tap_cell(1, 4);
  EXPECT(q.game.playing() && q.game.cell(1, 4) == 1);
}

void test_pause_wipes_and_resumes_stopped() {
  Rig r;
  r.game.update(G::kIntroMs);
  r.tap(G::kRecordButton);
  const int wipes = r.sampler.wipes;
  r.game.on_pause();
  EXPECT(r.sampler.wipes == wipes + 1);
  EXPECT(r.game.phase() == Phase::PLAY && !r.game.playing());
  r.fb.clear();
  r.game.on_resume();
  EXPECT(r.sampler.last_builtin == 0 && r.sampler.fx == 0);
  r.game.update(3000);
  EXPECT(!r.game.playing() && r.fb.count == 0);  // no surprise sound on resume
  EXPECT(r.game.cell(2, 0) == 1);  // pattern kept
}

void test_privacy_wipe_and_exit() {
  Rig r;
  r.game.update(G::kIntroMs);
  int wipes = r.sampler.wipes;
  r.game.privacy_wipe();
  EXPECT(r.sampler.wipes == wipes + 1 && !r.game.playing());
  wipes = r.sampler.wipes;
  r.game.exit();
  EXPECT(r.sampler.wipes == wipes + 1 && !r.game.active());
  r.game.privacy_wipe();  // not running: nothing to do
  EXPECT(r.sampler.wipes == wipes + 1);
  r.game.update(1000);
  EXPECT(r.game.steps_played() == 1);
}

void test_reentry_resets_pattern_and_tempo() {
  Rig r;
  r.stopped();
  r.tap_cell(1, 1);
  r.tap(G::kTempoButton);
  r.game.exit();
  r.game.enter(r.ctx);
  EXPECT(r.game.cell(1, 1) == 0 && r.game.cell(2, 0) == 1 && r.game.tempo() == G::kDefaultTempo);
}

void test_scene_manager_runs_beat_sharks() {
  Rng rng{1};
  testing::RecordingFeedback fb;
  FakeSampler sampler;
  G game(&sampler);
  GameEntry entries[] = {{GameId::BEAT_SHARKS, Intent::UNKNOWN, &game}};
  SceneManager sm(entries, 1, fb);
  sm.handle(make_touch_event(static_cast<int16_t>(SceneManager::kTileRects[0].x + 10),
                             static_cast<int16_t>(SceneManager::kTileRects[0].y + 10), 0));
  EXPECT(game.active());
  sm.update(G::kIntroMs);
  EXPECT(game.playing());
  const int wipes = sampler.wipes;
  sm.handle(make_touch_event(10, 10, 1000));  // Home
  EXPECT(!game.active() && sampler.wipes == wipes + 1);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_enter_has_starter_beat_and_clean_sampler();
  test_tap_cycles_three_sounds_then_empty();
  test_voice_row_is_mono();
  test_no_sampler_voice_row_uses_synth_fallback();
  test_step_clock_has_no_drift();
  test_loop_plays_each_column_in_order();
  test_stop_parks_and_play_restarts();
  test_tempo_changes_next_step_without_restart();
  test_preview_skipped_when_fin_is_arriving();
  test_full_ocean_celebrates_once_and_rearms();
  test_recording_pauses_loop_autostops_and_places_sharks();
  test_recording_keeps_existing_voice_sharks();
  test_too_quiet_wiggles_without_failure_sound();
  test_recording_timeout_backstop();
  test_mic_off_sparkle_cycles_builtins();
  test_voice_intents();
  test_intro_tap_on_play_does_not_stop();
  test_pause_wipes_and_resumes_stopped();
  test_privacy_wipe_and_exit();
  test_reentry_resets_pattern_and_tempo();
  test_scene_manager_runs_beat_sharks();
  return testing::finish("beat_sharks_test");
}
