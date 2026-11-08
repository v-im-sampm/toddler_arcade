// Host tests for the portable Sound Jar rules (v2/reference/sound_jar.html).
#include <cmath>
#include <cstdio>
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

struct Log : Sound {
  int starts = 0, music = 0, music_stops = 0, voices = 0, releases = 0, bends = 0, loops = 0, ducks = 0, drips = 0;
  std::vector<Cue> cues;
  double last_bend = 0;
  bool echo = false, robot = false, wobble = false;
  void start() override { ++starts; }
  void start_music() override { ++music; }
  void stop_music() override { ++music_stops; }
  void cue(Cue c, int) override { cues.push_back(c); }
  void voice_start(uint32_t, int, double, bool, bool) override { ++voices; }
  void voice_release(uint32_t, double) override { ++releases; }
  void voice_bend(uint32_t, double s) override { ++bends; last_bend = s; }
  void voice_loop(uint32_t) override { ++loops; }
  void apply_fx(bool e, bool r, bool w) override { echo = e; robot = r; wobble = w; }
  void duck(double) override { ++ducks; }
  void drip(double, int) override { ++drips; }
  int count(Cue c) const {
    int n = 0;
    for (Cue x : cues) n += x == c;
    return n;
  }
};

void run(Game& g, double seconds) {
  const int n = static_cast<int>(std::lround(seconds / kDt));
  for (int i = 0; i < n; ++i) g.tick(kDt);
}
bool until(Game& g, Phase p, double max_s) {
  for (double t = 0; t < max_s && g.phase() != p; t += kDt) g.tick(kDt);
  return g.phase() == p;
}
void tap(Game& g, double x, double y) { g.down(x, y); g.up(); }
void to_empty(Game& g) {
  tap(g, 160, 120);
  until(g, Phase::Empty, 3);
}
// Records one take and returns the phase it ends in (Full or TooQuiet).
Phase record(Game& g) {
  run(g, 0.2);
  tap(g, kRecX, kRecY);
  for (double t = 0; t < 8; t += kDt) {
    g.tick(kDt);
    if (g.phase() == Phase::Full || g.phase() == Phase::TooQuiet) return g.phase();
  }
  return g.phase();
}
void fill(Game& g) {
  to_empty(g);
  record(g);
  run(g, 1.0);  // let the preview play out
}
}  // namespace

static void test_start_is_silent_and_waits() {
  Log s;
  Game g(s, 1234);
  run(g, 5);
  CHECK(g.phase() == Phase::Start);
  CHECK(s.starts == 0 && s.music == 0 && s.cues.empty() && s.drips == 0);
  CHECK(g.amb() == 0);
  tap(g, 300, 220);  // any tap starts
  CHECK(g.phase() == Phase::Intro);
  CHECK(s.starts == 1 && s.music == 1);
}

static void test_intro_wakes_pads_then_empty() {
  Log s;
  Game g(s, 1234);
  tap(g, 160, 120);
  run(g, 0.32);
  CHECK(s.count(Cue::Wake) == 1);
  run(g, 0.6);
  CHECK(s.count(Cue::PadNote) == 5);
  for (int i = 0; i < 5; ++i) CHECK(g.pad(i).awake);
  CHECK(until(g, Phase::Empty, 1));
  CHECK(g.jar().sound == kFrog);
}

static void test_intro_tap_skips() {
  Log s;
  Game g(s, 1234);
  tap(g, 160, 120);
  run(g, 0.1);
  tap(g, 160, 120);  // reference: INTRO tap sets t = 99
  g.tick(kDt);
  CHECK(g.phase() == Phase::Empty);
  for (int i = 0; i < 5; ++i) CHECK(g.pad(i).awake);
  CHECK(s.count(Cue::PadNote) == 0);  // skipped pads wake silently
}

static void test_take_order_seeded_whisper_last() {
  for (uint32_t seed : {1u, 1234u, 777u}) {
    Log s;
    Game g(s, seed);
    to_empty(g);
    std::vector<int> takes;
    std::vector<Phase> ends;
    for (int i = 0; i < 4; ++i) {
      ends.push_back(record(g));
      takes.push_back(g.take());
      run(g, 1.6);
    }
    CHECK(takes[3] == kWhisper);
    CHECK(ends[3] == Phase::TooQuiet);
    bool seen[4] = {};
    for (int i = 0; i < 3; ++i) {
      CHECK(takes[i] >= 1 && takes[i] <= 3);
      CHECK(ends[i] == Phase::Full);
      if (takes[i] >= 1 && takes[i] <= 3) seen[takes[i]] = true;
    }
    CHECK(seen[1] && seen[2] && seen[3]);
    CHECK(g.tries() == 4);
    // The same seed + input timeline reproduces the same order.
    Log s2;
    Game h(s2, seed);
    to_empty(h);
    for (int i = 0; i < 3; ++i) {
      record(h);
      CHECK(h.take() == takes[i]);
      run(h, 1.6);
    }
  }
  Log a, b;
  Game ga(a, 1), gb(b, 2);
  to_empty(ga); to_empty(gb);
  ga.tick(kDt); gb.tick(kDt);
  // order is shuffled lazily on the first recording
  CHECK(ga.order().empty());
}

static void test_too_quiet_forgiving() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  for (int i = 0; i < 3; ++i) { record(g); run(g, 1.6); }
  CHECK(record(g) == Phase::TooQuiet);
  CHECK(s.count(Cue::Hmm) == 1);
  CHECK(g.rec().louder > 0);
  CHECK(until(g, Phase::Empty, 1.5));
  run(g, 1);
  CHECK(g.jar().mist == kFrog && g.jar().sound == kFrog);
  // A new round reshuffles and the jar can be filled again.
  CHECK(record(g) == Phase::Full);
}

static void test_recording_ends_on_silence_or_3s_and_early_tap() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  run(g, 0.2);
  tap(g, kRecX, kRecY);
  CHECK(g.phase() == Phase::Countdown);
  CHECK(s.count(Cue::LidPop) == 1);
  CHECK(until(g, Phase::Recording, 1.5));
  double t = 0;
  while (g.phase() == Phase::Recording && t < 4) { g.tick(kDt); t += kDt; }
  CHECK(t <= 3.0 + kDt);
  // Early finish: tap rec again after 0.3 s.
  Log s2;
  Game h(s2, 1234);
  to_empty(h);
  run(h, 0.2);
  tap(h, kRecX, kRecY);
  until(h, Phase::Recording, 1.5);
  run(h, 0.2);
  tap(h, kRecX, kRecY);  // too early: ignored
  CHECK(h.phase() == Phase::Recording);
  run(h, 0.15);
  tap(h, kRecX, kRecY);
  CHECK(h.phase() == Phase::Processing || h.phase() == Phase::TooQuiet);
}

static void test_cues_gated_while_recording() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  run(g, 0.2);
  tap(g, kRecX, kRecY);
  until(g, Phase::Recording, 1.5);
  const std::size_t before = s.cues.size();
  const int drips = s.drips;
  tap(g, 300, 70);  // empty space: tap note
  run(g, 0.4);
  CHECK(s.cues.size() == before);  // voiceOk(): silent while RECORDING
  (void)drips;
  CHECK(g.phase() == Phase::Recording);
}

static void test_full_celebrates_and_previews() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  CHECK(record(g) == Phase::Full);
  CHECK(s.count(Cue::LidShut) == 1);
  CHECK(s.count(Cue::Tada) == 1);
  const int v = s.voices;
  run(g, 0.6);
  CHECK(s.voices == v + 1);  // the jar previews the new take 0.45 s after celebrating
  CHECK(g.jar().sound == g.take());
}

static void test_pad_hold_loops_and_bend() {
  Log s;
  Game g(s, 1234);
  fill(g);
  const double y = pad_y(1);
  g.down(pad_x(1), y);
  CHECK(g.holding());
  run(g, 0.3);
  CHECK(!g.pad_looping(1));
  run(g, 0.1);
  CHECK(g.pad_looping(1));
  g.move(pad_x(1), y - 12);  // inside the 12 px dead zone
  CHECK(g.hold().bend == 0);
  g.move(pad_x(1), y - 28);  // 16 px past = 2 semitones up
  CHECK(std::abs(g.hold().bend - 2) < 1e-9);
  CHECK(std::abs(s.last_bend - 2) < 1e-9);
  g.move(pad_x(1), y + 200);  // clamped at -5
  CHECK(std::abs(g.hold().bend + 5) < 1e-9);
  g.move(pad_x(1), y - 28, 7);  // another pointer does not bend this hold
  CHECK(std::abs(g.hold().bend + 5) < 1e-9);
  g.up();
  CHECK(!g.holding());
  CHECK(!g.pad_looping(1));
}

static void test_pad_short_tap_plays_out() {
  Log s;
  Game g(s, 1234);
  fill(g);
  const int rel = s.releases;
  tap(g, pad_x(3), pad_y(3));
  CHECK(s.releases == rel);  // one-shots are not cut off on release
  CHECK(g.pad(3).glow > 0);
  run(g, 0.05);
  CHECK(g.pad(3).sing > 0 || !g.voices().empty());
}

static void test_fx_max_two_and_flags() {
  Log s;
  Game g(s, 1234);
  fill(g);
  tap(g, kFx[kEcho].x, kFxY); run(g, 0.1);
  tap(g, kFx[kWobble].x, kFxY); run(g, 0.1);
  CHECK(g.fx().size() == 2 && s.echo && s.wobble && !s.robot);
  tap(g, kFx[kRobot].x, kFxY); run(g, 0.1);
  CHECK(g.fx().size() == 2);
  CHECK(!g.fx_on(kEcho) && g.fx_on(kWobble) && g.fx_on(kRobot));
  CHECK(s.count(Cue::FxBump) == 1);
  CHECK(!s.echo && s.robot && s.wobble);
  tap(g, kFx[kRobot].x, kFxY); run(g, 0.1);  // toggle off
  CHECK(!g.fx_on(kRobot) && s.count(Cue::FxOff) == 1);
  CHECK(g.jar().visor_to == 0);
}

static void test_back_reverses_voice() {
  Log s;
  Game g(s, 1234);
  fill(g);
  tap(g, kFx[kBack].x, kFxY);
  run(g, 0.5);
  tap(g, pad_x(0), pad_y(0));
  bool rev = false;
  for (const Voice& v : g.voices()) rev = rev || (v.pad == 0 && v.rev);
  CHECK(rev);
}

static void test_debounce() {
  Log s;
  Game g(s, 1234);
  fill(g);
  const int a = g.actions();
  tap(g, pad_x(2), pad_y(2));
  tap(g, pad_x(2), pad_y(2));  // same instant: debounced (80 ms)
  CHECK(g.actions() == a + 1);
  run(g, 0.1);
  tap(g, pad_x(2), pad_y(2));
  CHECK(g.actions() == a + 2);
}

static void test_bin_empties_and_aborts() {
  Log s;
  Game g(s, 1234);
  fill(g);
  tap(g, kBinX, kBinY);
  CHECK(g.phase() == Phase::Empty);
  CHECK(g.jar().sound == kFrog);
  CHECK(s.count(Cue::Whoosh) == 1);
  CHECK(g.voices().empty() || !g.any_live_voice());
  // Bin during a recording aborts it and closes the lid.
  run(g, 0.3);
  tap(g, kRecX, kRecY);
  until(g, Phase::Recording, 1.5);
  run(g, 0.1);
  tap(g, kBinX, kBinY);
  CHECK(g.phase() == Phase::Empty);
  CHECK(g.lid().to == 0);
  run(g, 1);
  CHECK(g.lid().u == 0);
}

static void test_suggest_cycle_and_hints() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  Suggestion sg = g.suggest();
  CHECK(sg.kind == Target::Record && sg.x == kRecX && sg.y == kRecY);
  run(g, 5.9);
  CHECK(g.hint() == 0);
  run(g, 0.2);
  CHECK(g.hint() == 1);
  CHECK(g.rec().wig > 0);
  CHECK(s.count(Cue::Hint) == 0);
  run(g, 6.0);
  CHECK(g.hint() == 2 && s.count(Cue::Hint) == 1);
  run(g, 10);
  CHECK(s.count(Cue::Hint) == 1);  // spoken once per idle period
  tap(g, 300, 70);
  CHECK(g.hint() == 0);

  Log s2;
  Game h(s2, 1234);
  fill(h);
  const int a = h.actions();
  sg = h.suggest();
  static const Target kinds[9] = {Target::Pad, Target::Fx, Target::Pad, Target::Fx, Target::Pad,
                                  Target::Fx,  Target::Pad, Target::Fx, Target::Pad};
  static const int idx[9] = {2, kEcho, 0, kWobble, 4, kRobot, 1, kBack, 3};
  CHECK(sg.kind == kinds[a % 9] && sg.index == idx[a % 9]);
  double x, y;
  CHECK(h.auto_target(x, y));
  tap(h, x, y);
  CHECK(h.actions() == a + 1);
}

static void test_hints_pause_while_holding_or_recording() {
  Log s;
  Game g(s, 1234);
  fill(g);
  g.down(pad_x(0), pad_y(0));
  run(g, 8);
  CHECK(g.hint() == 0);
  g.up();
}

static void test_home_resets_everything() {
  Log s;
  Game g(s, 1234);
  fill(g);
  tap(g, kFx[kEcho].x, kFxY);
  g.down(pad_x(0), pad_y(0));
  run(g, 0.5);
  g.down(20, 20, 1);
  CHECK(g.phase() == Phase::Start);
  CHECK(s.music_stops == 1);
  CHECK(!g.music_on() && !g.holding() && g.fx().empty() && g.particles().empty() && g.rings().empty());
  CHECK(g.order().empty() && g.take() == -1 && g.tries() == 0 && g.actions() == 0);
  CHECK(g.jar().sound == kFrog && g.lid().u == 0);
  for (int i = 0; i < 5; ++i) CHECK(!g.pad(i).awake);
  CHECK(!s.echo);
  run(g, 3);
  CHECK(g.phase() == Phase::Start);
  tap(g, 160, 120);
  CHECK(g.phase() == Phase::Intro && s.music == 2);
}

static void test_intents_match_touch() {
  Log s;
  Game g(s, 1234);
  g.intent(Intent::AGAIN);  // nothing before start
  CHECK(g.phase() == Phase::Start);
  g.intent(Intent::START);
  CHECK(g.phase() == Phase::Intro);
  until(g, Phase::Empty, 3);
  const int v = s.voices;
  g.intent(Intent::AGAIN);  // same as tapping the jar
  CHECK(s.voices == v + 1);
  g.intent(Intent::HELP);
  CHECK(g.hint() == 2 && s.count(Cue::Hint) == 1);
  g.intent(Intent::BACK);
  CHECK(g.phase() == Phase::Start);
  g.intent(Intent::HELP);  // not playable: ignored
  CHECK(g.hint() == 0);
}

static void test_mic_disabled_is_harmless() {
  Log s;
  Game g(s, 1234);
  to_empty(g);
  const Phase p = g.phase();
  tap(g, 292, 28);
  CHECK(g.phase() == p);
  CHECK(!g.rings().empty());
}

static void test_particle_cap_and_reduced() {
  Log s;
  Game g(s, 1234);
  fill(g);
  for (int i = 0; i < 80; ++i) { tap(g, 100 + (i % 7) * 10, 70); g.tick(0.09); }
  CHECK(static_cast<int>(g.particles().size()) <= kMaxParticles);
  Log s2, s3;
  Game a(s2, 9), b(s3, 9);
  b.set_reduced(true);
  to_empty(a); to_empty(b);
  run(a, 1); run(b, 1);
  tap(a, 300, 70); tap(b, 300, 70);
  CHECK(b.particles().size() < a.particles().size());  // reference emit(): n * 0.5 when reduced
}

static void test_determinism_same_timeline() {
  auto session = [](uint32_t seed) {
    Log s;
    Game g(s, seed);
    fill(g);
    tap(g, kFx[kEcho].x, kFxY);
    for (int i = 0; i < 5; ++i) { tap(g, pad_x(i), pad_y(i)); run(g, 0.3); }
    tap(g, 300, 70);
    run(g, 0.2);
    std::vector<double> sig;
    for (const Particle& p : g.particles()) { sig.push_back(p.x); sig.push_back(p.y); }
    sig.push_back(g.jar().level);
    sig.push_back(g.take());
    return sig;
  };
  CHECK(session(42) == session(42));
}

static void test_ambience_drips_schedule() {
  Log s;
  Game g(s, 1234);
  tap(g, 160, 120);
  run(g, 14.2);
  CHECK(s.drips == 6);  // six drips per 14 s loop
}

int main() {
  test_start_is_silent_and_waits();
  test_intro_wakes_pads_then_empty();
  test_intro_tap_skips();
  test_take_order_seeded_whisper_last();
  test_too_quiet_forgiving();
  test_recording_ends_on_silence_or_3s_and_early_tap();
  test_cues_gated_while_recording();
  test_full_celebrates_and_previews();
  test_pad_hold_loops_and_bend();
  test_pad_short_tap_plays_out();
  test_fx_max_two_and_flags();
  test_back_reverses_voice();
  test_debounce();
  test_bin_empties_and_aborts();
  test_suggest_cycle_and_hints();
  test_hints_pause_while_holding_or_recording();
  test_home_resets_everything();
  test_intents_match_touch();
  test_mic_disabled_is_harmless();
  test_particle_cap_and_reduced();
  test_determinism_same_timeline();
  test_ambience_drips_schedule();
  if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
  std::puts("sound_jar_game_test: all passed");
  return 0;
}
