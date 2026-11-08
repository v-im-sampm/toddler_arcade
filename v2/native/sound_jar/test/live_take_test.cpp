// Host tests for the Sound Jar microphone take (game/live_take.hpp) and its use by the game. The "audio thread" is
// simulated by feeding LiveTake::capture() synchronously between game ticks; no hardware is involved.
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "game/sound_jar.hpp"

using namespace sound_jar;
using little_arcade::Intent;

static int g_failures = 0;
#define CHECK(cond)                                                                              \
  do {                                                                                           \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
  } while (0)

namespace {
constexpr double kDt = 1.0 / 64;
constexpr int kSr = kTakeSampleRate;
using Signal = std::function<double(double)>;  // amplitude (int16 units) at seconds since the capture started

struct Log : Sound {
  std::vector<int> takes;
  int lid_pop = 0;
  void voice_start(uint32_t, int take, double, bool, bool) override { takes.push_back(take); }
  void cue(Cue c, int) override { lid_pop += c == Cue::LidPop; }
};

struct Rig {
  std::vector<int16_t> clip = std::vector<int16_t>(LiveTake::kMaxSamples);
  LiveTake mic{clip.data()};
  Signal signal = [](double) { return 0.0; };
  Signal noise = [](double) { return 0.0; };  // RMS (int16 units) of a broadband room noise under the signal
  int pos = 0;                                 // samples offered since the capture started (lead-in included)
  uint32_t lcg = 12345;
  // One audio block: what follows so far (a 440 Hz tone of the signal's level, plus the noise).
  void feed(int n) {
    mic.capture(nullptr, 0);  // apply pending requests first, so a new capture starts its own clock
    if (mic.state() != LiveTake::State::Recording) pos = 0;
    std::vector<int16_t> block(n);
    for (int i = 0; i < n; ++i) {
      const double t = (pos + i) / static_cast<double>(kSr);
      lcg = lcg * 1664525u + 1013904223u;
      const double u = (lcg >> 8) / 16777216.0 * 2 - 1;  // uniform -1..1 (RMS 1/sqrt 3)
      block[i] = static_cast<int16_t>(std::lround(signal(t) * std::sin(2 * 3.14159265358979 * 440 * t) +
                                                  noise(t) * std::sqrt(3.0) * u));
    }
    pos += n;
    mic.capture(block.data(), block.size());
  }
  bool all_zero() const {
    for (int16_t s : clip)
      if (s) return false;
    return true;
  }
};

void tick(Game& g, Rig& r, double seconds) {
  const int n = static_cast<int>(std::lround(seconds / kDt));
  for (int i = 0; i < n; ++i) {
    r.feed(static_cast<int>(kDt * kSr));
    g.tick(kDt);
  }
}
bool until(Game& g, Rig& r, Phase p, double max_s) {
  for (double t = 0; t < max_s && g.phase() != p; t += kDt) tick(g, r, kDt);
  return g.phase() == p;
}
void tap(Game& g, double x, double y) { g.down(x, y); g.up(); }
Signal burst(double from, double to, double amp) {
  return [=](double t) { return t >= from && t < to ? amp : 0.0; };
}
// Starts the game with a mic of the given availability and waits for the empty jar.
void begin(Game& g, Rig& r) {
  g.set_mic(&r.mic);
  tap(g, 160, 120);
  until(g, r, Phase::Empty, 3);
}
}  // namespace

// ---------------- LiveTake on its own ----------------
static void test_capture_trim_normalise_fade() {
  Rig r;
  r.mic.set_hardware(true);
  r.signal = burst(0.4, 0.9, 3000);  // 0.4 s of silence, 0.5 s of voice, then silence
  r.mic.arm();
  r.feed(0);
  CHECK(r.mic.state() == LiveTake::State::Recording);
  CHECK(r.mic.capturing());
  while (!r.mic.should_stop() && r.mic.elapsed() < 3.2) r.feed(256);
  // ends 0.5 s after the last active 20 ms frame (reference end rule), not at the 3 s cap; the first 0.1 s
  // offered is the discarded lead-in, so the voice ends at 0.8 s of kept audio
  CHECK(r.mic.elapsed() > 1.28 && r.mic.elapsed() < 1.4);
  CHECK(std::abs(r.mic.voiced() - 0.5) < 0.05);
  CHECK(r.mic.level() < 0.002);
  CHECK(!r.mic.finalize());  // not before the audio thread acknowledged the stop
  r.mic.stop();
  r.feed(0);
  CHECK(r.mic.state() == LiveTake::State::Captured);
  CHECK(!r.mic.capturing());
  CHECK(r.mic.finalize());
  CHECK(r.mic.ready());
  const TakeData& d = r.mic.data();
  // trimmed to the voice, one 20 ms frame before and two after
  CHECK(d.samples >= static_cast<int>(0.5 * kSr) && d.samples <= static_cast<int>(0.58 * kSr));
  CHECK(std::abs(d.dur - d.samples / static_cast<double>(kSr)) < 1e-9);
  CHECK(d.frames == (d.samples + 319) / 320);
  int peak = 0;
  for (int i = 0; i < d.samples; ++i) peak = std::max(peak, std::abs(static_cast<int>(d.pcm[i])));
  CHECK(std::abs(peak - 16384) <= 300);  // -6 dBFS
  CHECK(d.pcm[0] == 0 && d.pcm[d.samples - 1] == 0);  // 5 ms fades
  for (int i = d.samples; i < LiveTake::kMaxSamples; ++i)
    if (r.clip[i]) { CHECK(!"raw input left after the clip"); break; }
  // play_lev is the processed clip's envelope (the reference's lev * kp)
  double mx = 0;
  for (int f = 0; f < d.frames; ++f) mx = std::max(mx, d.play_lev[f]);
  CHECK(mx > 0.25 && mx < 0.4);
  CHECK(r.mic.takes() == 1);
}

static void test_three_second_cap() {
  Rig r;
  r.mic.set_hardware(true);
  r.signal = [](double) { return 2000.0; };
  r.mic.arm();
  for (int i = 0; i < 300; ++i) r.feed(256);  // 4.8 s offered
  CHECK(r.mic.elapsed() == 3.0);
  CHECK(r.mic.should_stop());
  CHECK(r.mic.state() == LiveTake::State::Captured);
  CHECK(r.mic.capturing());  // still silent until the game stops it
  r.mic.stop();
  r.feed(0);
  CHECK(r.mic.finalize());
  CHECK(r.mic.data().samples == LiveTake::kMaxSamples);
}

static void test_wipe_and_rearm_zero_everything() {
  Rig r;
  r.mic.set_hardware(true);
  r.signal = burst(0, 0.6, 5000);
  r.mic.arm();
  for (int i = 0; i < 80; ++i) r.feed(256);
  r.mic.stop();
  r.feed(0);
  CHECK(r.mic.finalize());
  CHECK(!r.all_zero());
  const uint32_t gen = r.mic.generation();
  r.mic.wipe();
  CHECK(!r.mic.ready());
  CHECK(r.mic.data().frames == 0 && r.mic.data().samples == 0 && r.mic.data().dur == 0);
  r.feed(0);  // applied on the audio thread
  CHECK(r.all_zero());
  CHECK(r.mic.generation() != gen);
  CHECK(r.mic.state() == LiveTake::State::Idle);
  CHECK(!r.mic.capturing());
  for (int f = 0; f < LiveTake::kMaxFrames; ++f)
    if (r.mic.data().lev[f] || r.mic.data().play_lev[f]) { CHECK(!"envelope survived the wipe"); break; }
  // a new recording also starts from a zeroed buffer
  r.mic.arm();
  for (int i = 0; i < 80; ++i) r.feed(256);
  r.mic.stop();
  r.feed(0);
  CHECK(r.mic.finalize());
  r.mic.arm();
  r.signal = [](double) { return 0.0; };
  r.feed(0);
  CHECK(r.all_zero());
}

static void test_silence_and_dc_are_not_voice() {
  Rig r;
  r.mic.set_hardware(true);
  std::vector<int16_t> dc(256, 1500);  // a constant offset (INMP441 DC) must not count as sound
  for (int i = 0; i < 20; ++i) r.mic.capture(dc.data(), dc.size());  // the mic runs before any recording
  r.mic.arm();
  for (int i = 0; i < 200; ++i) r.mic.capture(dc.data(), dc.size());
  CHECK(r.mic.voiced() == 0);
  r.mic.stop();
  r.feed(0);
  CHECK(!r.mic.finalize());  // nothing to keep: zeroed and idle
  CHECK(r.all_zero());
  CHECK(r.mic.state() == LiveTake::State::Idle);
}

static void test_availability() {
  Rig r;
  CHECK(!r.mic.available());  // no hardware
  r.mic.set_hardware(true);
  CHECK(r.mic.available());
  r.mic.set_allowed(false);  // parent switch off
  CHECK(!r.mic.available());
}

// ---------------- the game with a microphone ----------------
static void test_game_records_live_take() {
  Log s;
  Game g(s, 1234);
  Rig r;
  r.mic.set_hardware(true);
  begin(g, r);
  CHECK(g.mic_live());
  r.signal = burst(0.1, 0.8, 4000);
  const int pops = s.lid_pop;
  tap(g, kRecX, kRecY);
  // native, user-requested: a live take records from the tap (no countdown, and no lid-pop sound)
  CHECK(g.phase() == Phase::Recording);
  CHECK(g.recording_live());
  CHECK(s.lid_pop == pops);
  CHECK(g.lid().to == 1);
  tick(g, r, kDt);
  CHECK(r.mic.capturing());  // speaker silent while capturing
  bool saw_level = false;
  while (g.phase() == Phase::Recording) {
    tick(g, r, kDt);
    saw_level |= g.input_level() > 0.05;  // the jar's mist follows the real input
  }
  CHECK(saw_level);
  CHECK(g.phase() == Phase::Processing);
  r.feed(0);  // the next audio block applies the stop
  CHECK(!r.mic.capturing());
  CHECK(until(g, r, Phase::Full, 2));
  CHECK(g.live_clip());
  CHECK(std::abs(g.voiced() - 0.7) < 0.05);
  tick(g, r, 1.0);  // the preview plays the live take
  CHECK(!s.takes.empty() && s.takes.back() == kLiveTake);
  s.takes.clear();
  tap(g, pad_x(0), pad_y(0));
  CHECK(!s.takes.empty() && s.takes.back() == kLiveTake);
  CHECK(!g.voices().empty() && std::abs(g.voices().back().dur - r.mic.data().dur) < 1e-9);
  CHECK(r.mic.takes() == 1);
}

static void test_game_too_quiet_is_forgiving_and_wiped() {
  Log s;
  Game g(s, 1234);
  Rig r;
  r.mic.set_hardware(true);
  begin(g, r);
  r.signal = [](double) { return 300.0; };  // ~212 RMS: a steady sound, never LOUD (512)
  tap(g, kRecX, kRecY);
  CHECK(until(g, r, Phase::TooQuiet, 6));
  CHECK(!g.live_clip());
  tick(g, r, kDt);
  CHECK(r.all_zero());
  // native, user-requested: the jar stays empty and silent, and the hand points at the microphone
  CHECK(g.hushed());
  CHECK(g.jar().level_to == 0);
  CHECK(until(g, r, Phase::Empty, 2));
  CHECK(g.suggest().kind == Target::Record);
  CHECK(g.hint() >= 2);
  s.takes.clear();
  tap(g, kJarX, kJarY);
  tap(g, pad_x(2), pad_y(2));
  g.intent(Intent::AGAIN);
  tick(g, r, 0.5);
  CHECK(s.takes.empty());  // no built-in frog
  CHECK(g.hint() >= 2);    // a tap elsewhere does not dismiss the pointer
  r.signal = burst(0.1, 0.8, 4000);  // the next recording clears it
  tap(g, kRecX, kRecY);
  CHECK(!g.hushed());
  CHECK(until(g, r, Phase::Full, 6));
  CHECK(g.live_clip());
  tick(g, r, 1.0);
  CHECK(!s.takes.empty() && s.takes.back() == kLiveTake);
}

// Bin and Home after a silent take return to the reference's starting jar (the built-in frog).
static void test_hush_cleared_by_bin_and_home() {
  for (bool use_bin : {true, false}) {
    Log s;
    Game g(s, 1234);
    Rig r;
    r.mic.set_hardware(true);
    begin(g, r);
    tap(g, kRecX, kRecY);  // silence
    CHECK(until(g, r, Phase::Empty, 6));
    CHECK(g.hushed());
    if (use_bin) {
      tap(g, kBinX, kBinY);
    } else {
      tap(g, 20, 20);
      begin(g, r);
    }
    CHECK(!g.hushed());
    tick(g, r, 1.0);
    s.takes.clear();
    tap(g, kJarX, kJarY);
    CHECK(!s.takes.empty() && s.takes.back() == kFrog);
  }
}

// Room noise must neither keep a take running to 3 s nor be kept. The profile is the device's (INMP441 at x16):
// a -55 dBFS floor with frames up to -45 dBFS, both above the reference ACTIVE (0.002 = -54 dBFS).
static void test_noise_floor_end_rule_and_trim() {
  Rig r;
  r.mic.set_hardware(true);
  r.noise = [](double t) { return (static_cast<int>(t / 0.02) % 7 == 3 ? 0.0056 : 0.0018) * 32768; };
  r.signal = burst(0.5, 1.1, 4000);
  r.mic.arm();
  while (!r.mic.should_stop() && r.mic.elapsed() < 3.2) r.feed(256);
  // voice ends at 1.0 s of kept audio; stop 0.5 s later, not at the cap
  CHECK(r.mic.elapsed() > 1.48 && r.mic.elapsed() < 1.6);
  CHECK(r.mic.floor_level() > 0.0015 && r.mic.floor_level() < 0.0021);
  r.mic.stop();
  r.feed(0);
  CHECK(r.mic.finalize());
  const TakeData& d = r.mic.data();
  CHECK(d.dur >= 0.6 && d.dur <= 0.68);  // the voice, one frame before and two after; none of the hiss
}

// The lead-in after arm (speaker tail, the tap's own click) is never kept and never counts as voice.
static void test_lead_in_discarded() {
  Rig r;
  r.mic.set_hardware(true);
  r.signal = burst(0.0, 0.05, 8000);  // a click right after the tap (its DC-blocker tail decays within the lead-in)
  r.mic.arm();
  for (int i = 0; i < 60; ++i) r.feed(256);
  CHECK(r.mic.voiced() == 0);
  CHECK(r.mic.peak_level() < 0.001);
  CHECK(r.mic.elapsed() == (60 * 256 - LiveTake::kDiscardSamples) / static_cast<double>(kSr));
  r.mic.stop();
  r.feed(0);
  CHECK(!r.mic.finalize());
  CHECK(r.all_zero());
}

static void test_builtin_flow_keeps_countdown() {
  for (bool hw : {false, true}) {
    Log s;
    Game g(s, 1234);
    Rig r;
    r.mic.set_hardware(hw);
    r.mic.set_allowed(!hw);  // no hardware, or the parent switch off
    begin(g, r);
    const int pops = s.lid_pop;
    tap(g, kRecX, kRecY);
    CHECK(g.phase() == Phase::Countdown);  // the reference's staged recording
    CHECK(s.lid_pop == pops + 1);
    CHECK(!g.recording_live());
  }
}

static void test_game_wipe_triggers() {
  Log s;
  Game g(s, 1234);
  Rig r;
  r.mic.set_hardware(true);
  auto fill = [&] {
    r.signal = burst(0, 0.6, 4000);
    tap(g, kRecX, kRecY);
    until(g, r, Phase::Full, 6);
    CHECK(g.live_clip());
    CHECK(!r.all_zero());
  };
  begin(g, r);
  fill();
  tick(g, r, 1.0);
  tap(g, kBinX, kBinY);  // bin
  tick(g, r, kDt);
  CHECK(!g.live_clip() && r.all_zero());

  fill();
  tick(g, r, 1.0);
  tap(g, kRecX, kRecY);  // new recording
  tick(g, r, kDt);
  CHECK(g.phase() == Phase::Recording && r.all_zero());
  until(g, r, Phase::Full, 6);
  tick(g, r, 1.0);
  CHECK(!r.all_zero());
  tap(g, 20, 20);  // Home = exit
  tick(g, r, kDt);
  CHECK(g.phase() == Phase::Start && !g.live_clip() && r.all_zero());

  begin(g, r);  // bin during a live recording aborts and wipes it
  r.signal = [](double) { return 4000.0; };
  tap(g, kRecX, kRecY);
  until(g, r, Phase::Recording, 2);
  tick(g, r, 0.4);
  CHECK(r.mic.capturing());
  tap(g, kBinX, kBinY);
  tick(g, r, kDt);
  CHECK(g.phase() == Phase::Empty && !r.mic.capturing() && r.all_zero());
  tap(g, kRecX, kRecY);  // Home during a live recording stops and wipes it
  tick(g, r, 0.3);
  g.intent(Intent::BACK);
  tick(g, r, 2.0);
  CHECK(r.mic.state() == LiveTake::State::Idle && !r.mic.capturing() && r.all_zero());
}

static void test_early_tap_stops_live_recording() {
  Log s;
  Game g(s, 1234);
  Rig r;
  r.mic.set_hardware(true);
  begin(g, r);
  r.signal = [](double) { return 4000.0; };  // would run to the 3 s cap
  tap(g, kRecX, kRecY);
  until(g, r, Phase::Recording, 2);
  tick(g, r, 0.6);
  g.tick(0.1);  // past the reference's 0.3 s guard
  g.down(kRecX, kRecY);
  g.up();
  CHECK(g.phase() == Phase::Processing);
  CHECK(until(g, r, Phase::Full, 2));
  CHECK(g.live_clip());
  CHECK(r.mic.data().dur > 0.4 && r.mic.data().dur < 1.0);
}

static void test_intents_never_record() {
  Log s;
  Game g(s, 1234);
  Rig r;
  r.mic.set_hardware(true);
  begin(g, r);
  for (Intent in : {Intent::AGAIN, Intent::HELP, Intent::START, Intent::AGAIN}) {
    g.intent(in);
    tick(g, r, 2.0);
  }
  CHECK(r.mic.takes() == 0);
  CHECK(!r.mic.capturing());
  CHECK(g.phase() == Phase::Empty);
}

// SJ-5: with the mic absent or switched off the game is exactly the built-in game (same seed, same timeline).
static void test_unavailable_mic_is_builtin_game() {
  for (int variant = 0; variant < 3; ++variant) {
    Log a, b;
    Game plain(a, 77), with(b, 77);
    Rig r;
    if (variant == 1) r.mic.set_hardware(true), r.mic.set_allowed(false);
    if (variant == 2) r.mic.set_allowed(true);  // allowed but no hardware
    if (variant > 0) with.set_mic(&r.mic);
    for (Game* g : {&plain, &with}) tap(*g, 160, 120);
    for (int step = 0; step < 2400; ++step) {
      for (Game* g : {&plain, &with}) {
        if (step % 200 == 100 && g->can_play()) tap(*g, kRecX, kRecY);
        if (step % 200 == 180 && g->can_play()) tap(*g, pad_x(step % 5), pad_y(step % 5));
        g->tick(kDt);
      }
      CHECK(plain.phase() == with.phase());
      CHECK(plain.take() == with.take());
      CHECK(plain.jar().level == with.jar().level);
    }
    CHECK(a.takes == b.takes);
    CHECK(plain.tries() > 3);
    CHECK(r.mic.takes() == 0 && !with.live_clip());
  }
}

// The device's fixed input gain lifts quiet mic speech past LOUD; the peak meter reports the loudest frame.
static void test_input_gain_and_peak() {
  for (float gain : {1.f, 16.f}) {
    Rig r;
    r.mic.set_hardware(true);
    r.mic.set_input_gain(gain);
    r.signal = burst(0.0, 0.7, 60);  // ~ -58 dBFS peak: raw INMP441 speech at 30-50 cm (first 0.1 s discarded)
    r.mic.arm();
    for (int i = 0; i < 100; ++i) r.feed(256);
    const double expect = gain * 60 / std::sqrt(2.0) / 32768;
    CHECK(std::abs(r.mic.peak_level() - expect) < expect * 0.1);
    CHECK((r.mic.voiced() >= 0.5) == (gain > 1));
    r.mic.wipe();
    r.feed(0);
    CHECK(r.mic.peak_level() > 0);  // the calibration aggregate survives the privacy wipe; the samples do not
    CHECK(r.all_zero());
  }
}

int main() {
  test_capture_trim_normalise_fade();
  test_three_second_cap();
  test_wipe_and_rearm_zero_everything();
  test_silence_and_dc_are_not_voice();
  test_availability();
  test_game_records_live_take();
  test_game_too_quiet_is_forgiving_and_wiped();
  test_hush_cleared_by_bin_and_home();
  test_noise_floor_end_rule_and_trim();
  test_lead_in_discarded();
  test_builtin_flow_keeps_countdown();
  test_game_wipe_triggers();
  test_early_tap_stops_live_recording();
  test_intents_never_record();
  test_unavailable_mic_is_builtin_game();
  test_input_gain_and_peak();
  if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
  std::puts("sound_jar_live_test: all passed");
  return 0;
}
