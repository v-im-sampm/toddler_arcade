#include <cstdint>

#include "little_arcade/games/sound_jar.hpp"
#include "little_arcade/rng.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::sound_jar;

namespace {

// Records what the game asks of the sampler; the test plays the audio task.
class FakeSampler final : public ISampler {
 public:
  bool mic = true;
  bool recording = false;
  RecordResult pending = RecordResult::NONE;
  int wipes = 0, starts = 0, stops = 0, cancels = 0, plays = 0, loops = 0, bends = 0, end_loops = 0, releases = 0;
  int builtins = 0;
  uint8_t last_builtin = 0xFF;
  uint8_t fx = 0;
  uint32_t last_rate = 0, last_bend = 0;

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
    return r;
  }
  uint16_t record_progress_permille() const override { return 0; }
  uint8_t input_level() const override { return 0; }
  void play(uint32_t rate, bool loop) override {
    ++plays;
    loops += loop ? 1 : 0;
    last_rate = rate;
  }
  void bend(uint32_t rate) override {
    ++bends;
    last_bend = rate;
  }
  void end_loop() override { ++end_loops; }
  void release() override { ++releases; }
  void stop_all() override {}
  void set_effects(uint8_t mask) override { fx = mask; }
  void load_builtin(uint8_t i) override {
    ++builtins;
    last_builtin = i;
  }
  void wipe() override {
    ++wipes;
    recording = false;
    fx = 0;
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
  SoundJarGame game{&sampler};
  Rig() { game.enter(ctx); }
  void tap(const Rect& r) { game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), 0)); }
  void say(Intent i) { game.handle(make_voice_event(i, 0)); }
};

void test_layout_is_toddler_safe() {
  Rect all[2 + kEffectCount + kPadCount];
  std::size_t n = 0;
  all[n++] = SoundJarGame::kRecordButton;
  all[n++] = SoundJarGame::kJar;
  for (const Rect& r : SoundJarGame::kEffects) all[n++] = r;
  for (const Rect& r : SoundJarGame::kPads) all[n++] = r;
  for (std::size_t i = 0; i < n; ++i) {
    EXPECT(all[i].on_screen());
    EXPECT(all[i].large_enough());
    EXPECT(!all[i].overlaps(kHomeButton));
    EXPECT(!all[i].overlaps(kMicButton));
    for (std::size_t j = i + 1; j < n; ++j) EXPECT(!all[i].overlaps(all[j]));
  }
  for (std::size_t i = 1; i < kPadCount; ++i) EXPECT(SoundJarGame::kPadRates[i] > SoundJarGame::kPadRates[i - 1]);
  EXPECT(SoundJarGame::kPadRates[0] == ISampler::kRateMin && SoundJarGame::kPadRates[4] == ISampler::kRateMax);
  EXPECT(SoundJarGame::kPadRates[2] == ISampler::kRateUnity);
}

void test_enter_starts_with_a_boing_and_prompts() {
  Rig r;
  EXPECT(r.sampler.wipes == 1 && r.sampler.last_builtin == 0);
  EXPECT(r.game.phase() == Phase::INTRO);
  r.game.update(SoundJarGame::kIntroMs);
  EXPECT(r.game.phase() == Phase::PLAY);
  EXPECT(r.fb.count_of(Cue::PROMPT) == 1);
}

void test_pad_tap_plays_once_and_hold_loops() {
  Rig r;
  r.tap(SoundJarGame::kPads[4]);
  EXPECT(r.game.phase() == Phase::PLAY);  // input skips the intro
  EXPECT(r.fb.count_of(Cue::PROMPT) == 0);
  EXPECT(r.sampler.plays == 1 && r.sampler.loops == 1);
  EXPECT(r.sampler.last_rate == SoundJarGame::kPadRates[4]);
  EXPECT(r.game.pad_lit(4) && r.game.held_pad() == 4);
  r.game.update(100);
  r.game.pointer(false, 0, 0);  // short tap: the pass finishes on its own
  EXPECT(r.sampler.end_loops == 1 && r.sampler.releases == 0);
  EXPECT(r.game.held_pad() == -1);

  r.tap(SoundJarGame::kPads[0]);
  const int16_t x = static_cast<int16_t>(SoundJarGame::kPads[0].x + 28);
  for (int i = 0; i < 20; ++i) {
    r.game.update(30);
    r.game.pointer(true, x, 180);
  }
  EXPECT(r.game.looping());
  r.game.pointer(false, x, 180);
  EXPECT(r.sampler.releases == 1);
  EXPECT(!r.game.looping());
}

void test_slide_bends_between_pads() {
  Rig r;
  r.tap(SoundJarGame::kPads[0]);
  r.game.pointer(true, 32 + 5, 180);  // inside the dead zone: no bend
  EXPECT(r.sampler.bends == 0 && r.game.bend_position() == -1);
  r.game.pointer(true, 32 + 32, 180);  // halfway to pad 1
  EXPECT(r.sampler.bends == 1);
  EXPECT(r.game.bend_position() == 50);
  EXPECT(r.sampler.last_bend > SoundJarGame::kPadRates[0] && r.sampler.last_bend < SoundJarGame::kPadRates[1]);
  r.game.pointer(true, 32 + 32, 180);  // same spot: nothing new to send
  EXPECT(r.sampler.bends == 1);
  r.game.pointer(true, 319, 180);
  EXPECT(r.sampler.last_bend == ISampler::kRateMax);
  r.game.pointer(false, 319, 180);
  EXPECT(r.sampler.releases == 1);  // a bent note fades instead of replaying
  EXPECT(SoundJarGame::rate_for_position(-50) == ISampler::kRateMin);
  EXPECT(SoundJarGame::rate_for_position(200) == ISampler::kRateUnity);
}

void test_missing_pointer_stream_never_loops_forever() {
  Rig r;
  r.tap(SoundJarGame::kPads[2]);
  r.game.update(SoundJarGame::kPointerLostMs);
  EXPECT(r.game.held_pad() == -1);
  EXPECT(r.sampler.end_loops == 1);
}

void test_effects_toggle_stack_and_preview() {
  Rig r;
  r.tap(SoundJarGame::kEffects[0]);
  r.tap(SoundJarGame::kEffects[2]);
  EXPECT(r.game.effect_on(0) && !r.game.effect_on(1) && r.game.effect_on(2));
  EXPECT(r.sampler.fx == (ISampler::kFxReverse | ISampler::kFxRobot));
  EXPECT(r.sampler.plays == 2 && r.sampler.loops == 0);
  r.tap(SoundJarGame::kEffects[0]);
  EXPECT(r.sampler.fx == ISampler::kFxRobot);
  r.tap(SoundJarGame::kJar);
  EXPECT(r.game.jar_lit() && r.sampler.last_rate == ISampler::kRateUnity);
}

void test_recording_accepted_celebrates() {
  Rig r;
  r.game.update(SoundJarGame::kIntroMs);
  r.tap(SoundJarGame::kRecordButton);
  EXPECT(r.game.phase() == Phase::RECORDING && r.sampler.starts == 1);
  const int plays = r.sampler.plays;
  r.tap(SoundJarGame::kPads[1]);  // speaker is muted while recording
  r.say(Intent::AGAIN);
  EXPECT(r.sampler.plays == plays);
  r.tap(SoundJarGame::kRecordButton);
  r.tap(SoundJarGame::kRecordButton);
  EXPECT(r.sampler.stops == 1);
  r.game.update(16);
  EXPECT(r.game.phase() == Phase::RECORDING);
  r.sampler.pending = ISampler::RecordResult::ACCEPTED;
  r.game.update(16);
  EXPECT(r.game.phase() == Phase::CELEBRATE);
  EXPECT(r.game.sounds_caught() == 1);
  EXPECT(r.sampler.plays == plays + 1);  // the new sound plays once
  r.game.update(SoundJarGame::kCelebrateMs);
  EXPECT(r.game.phase() == Phase::PLAY);
}

void test_too_quiet_wiggles_without_failure_cue() {
  Rig r;
  r.tap(SoundJarGame::kRecordButton);
  r.sampler.pending = ISampler::RecordResult::TOO_QUIET;
  r.game.update(16);
  EXPECT(r.game.phase() == Phase::PLAY);
  EXPECT(r.game.wiggling());
  EXPECT(!r.fb.any(Cue::TRY_AGAIN));
  EXPECT(r.game.sounds_caught() == 0);
  r.game.update(SoundJarGame::kWiggleMs);
  EXPECT(!r.game.wiggling());
}

void test_recording_backstop_timeout() {
  Rig r;
  r.tap(SoundJarGame::kRecordButton);
  r.game.update(SoundJarGame::kRecordTimeoutMs);
  EXPECT(r.game.phase() == Phase::PLAY);
  EXPECT(r.sampler.cancels == 1);
}

void test_sparkle_mode_cycles_builtins() {
  Rig r;
  r.sampler.mic = false;
  EXPECT(r.game.sparkle_mode());
  r.tap(SoundJarGame::kRecordButton);
  EXPECT(r.sampler.starts == 0);
  EXPECT(r.sampler.last_builtin == 1 && r.game.phase() == Phase::CELEBRATE);
  r.game.update(SoundJarGame::kCelebrateMs);
  r.tap(SoundJarGame::kRecordButton);
  r.tap(SoundJarGame::kRecordButton);
  EXPECT(r.sampler.last_builtin == 0);
}

void test_privacy_wipes() {
  Rig r;
  r.tap(SoundJarGame::kRecordButton);
  r.game.on_pause();  // sleep or PAUSE mid-recording
  EXPECT(r.sampler.wipes == 2 && !r.sampler.recording);
  EXPECT(r.game.phase() == Phase::PLAY);
  r.tap(SoundJarGame::kEffects[1]);
  r.game.on_resume();
  EXPECT(r.sampler.fx == ISampler::kFxEcho);
  r.game.privacy_wipe();
  EXPECT(r.sampler.wipes == 3);
  r.game.exit();
  EXPECT(r.sampler.wipes == 4);
  EXPECT(!r.game.active());
}

void test_voice_again_and_help() {
  Rig r;
  r.say(Intent::AGAIN);  // nothing played yet: the jar sound
  EXPECT(r.sampler.last_rate == ISampler::kRateUnity && r.game.jar_lit());
  r.tap(SoundJarGame::kPads[3]);
  r.game.pointer(false, 0, 0);
  r.say(Intent::AGAIN);
  EXPECT(r.sampler.last_rate == SoundJarGame::kPadRates[3] && r.sampler.loops == 1);
  r.say(Intent::HELP);
  EXPECT(r.fb.count_of(Cue::PROMPT) == 1);
}

void test_works_without_audio() {
  Rng rng{3};
  testing::RecordingFeedback fb;
  GameContext ctx{rng, fb};
  SoundJarGame game;
  game.enter(ctx);
  EXPECT(game.sparkle_mode());
  game.handle(make_touch_event(100, 180, 0));  // pad 1
  EXPECT(fb.count_of(Cue::NOTE) == 1 && game.pad_lit(1));
  game.pointer(false, 0, 0);
  game.handle(make_touch_event(90, 10, 0));  // sparkle
  EXPECT(game.phase() == Phase::CELEBRATE && game.builtin_index() == 1);
  game.handle(make_touch_event(120, 90, 0));  // echo toggle
  EXPECT(game.effect_on(1));
  game.exit();
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_enter_starts_with_a_boing_and_prompts();
  test_pad_tap_plays_once_and_hold_loops();
  test_slide_bends_between_pads();
  test_missing_pointer_stream_never_loops_forever();
  test_effects_toggle_stack_and_preview();
  test_recording_accepted_celebrates();
  test_too_quiet_wiggles_without_failure_cue();
  test_recording_backstop_timeout();
  test_sparkle_mode_cycles_builtins();
  test_privacy_wipes();
  test_voice_again_and_help();
  test_works_without_audio();
  return testing::finish("sound_jar_test");
}
