#include "little_arcade/games/rainbow_piano.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::rainbow_piano;
using little_arcade::testing::RecordingFeedback;

namespace {

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  RainbowPianoGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(RainbowPianoGame::kIntroMs);
  }
  void tap(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2),
                                 t += 200));
  }
  void tap_key(std::size_t k) { tap(RainbowPianoGame::kKeys[k]); }
  void tap_instrument(std::size_t i) { tap(RainbowPianoGame::kInstrumentButtons[i]); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void follow_whole_tune() {
    for (std::size_t i = 0; i < RainbowPianoGame::kTuneLength; ++i) {
      tap_key(static_cast<std::size_t>(game.follow_key()));
    }
  }
};

Rect grown(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void test_layout_is_toddler_safe() {
  Rect all[kKeyCount + kInstrumentCount]{};
  std::size_t n = 0;
  for (const Rect& r : RainbowPianoGame::kKeys) all[n++] = r;
  for (const Rect& r : RainbowPianoGame::kInstrumentButtons) all[n++] = r;
  for (std::size_t i = 0; i < n; ++i) {
    const Rect& a = all[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton));
    EXPECT(a.y >= kPlayArea.y);
    for (std::size_t j = i + 1; j < n; ++j) EXPECT(!grown(a).overlaps(all[j]));
  }
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.follow_key() == -1);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(RainbowPianoGame::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.count_of(Cue::PROMPT) == 1);
  EXPECT(h.game.follow_key() >= 0 && h.game.follow_key() < static_cast<int>(kKeyCount));
}

void test_key_tap_during_intro_plays_immediately() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap_key(2);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.count == 1);  // no prompt talking over the first note
  EXPECT(h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant == note_variant(Instrument::BELLS, 2));
}

void test_every_key_emits_note_synchronously() {
  Harness h;
  h.enter_and_play();
  for (std::size_t k = 0; k < kKeyCount; ++k) {
    const int before = h.fb.count_of(Cue::NOTE);
    h.tap_key(k);  // no update() between press and assertion
    EXPECT(h.fb.count_of(Cue::NOTE) == before + 1);
    EXPECT(h.game.key_lit(k));
    EXPECT(h.game.last_key() == static_cast<int>(k));
  }
  EXPECT(h.game.notes_played() == kKeyCount);
  h.game.update(RainbowPianoGame::kKeyGlowMs);
  for (std::size_t k = 0; k < kKeyCount; ++k) EXPECT(!h.game.key_lit(k));
}

void test_three_instruments_give_distinct_notes() {
  Harness h;
  h.enter_and_play();
  bool seen[kInstrumentCount * kKeyCount] = {};
  for (std::size_t i = 0; i < kInstrumentCount; ++i) {
    h.tap_instrument(i);
    EXPECT(h.game.instrument() == static_cast<Instrument>(i));
    EXPECT(h.fb.last()->cue == Cue::TAP && h.fb.last()->variant == i);
    for (std::size_t k = 0; k < kKeyCount; ++k) {
      const int at = h.fb.count;
      h.tap_key(k);  // NOTE is always the first cue (a finished tune may add CELEBRATE)
      const uint8_t v = h.fb.entries[at].variant;
      EXPECT(h.fb.entries[at].cue == Cue::NOTE && v == note_variant(static_cast<Instrument>(i), k));
      EXPECT(v < kInstrumentCount * kKeyCount && !seen[v]);
      seen[v] = true;
    }
  }
  EXPECT(h.game.notes_played() == kInstrumentCount * kKeyCount);
}

void test_follow_light_completes_tune_and_celebrates() {
  Harness h;
  h.enter_and_play();
  for (std::size_t i = 1; i < RainbowPianoGame::kTuneLength; ++i) {
    EXPECT(h.game.tune_note(i) != h.game.tune_note(i - 1));
  }
  h.follow_whole_tune();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  EXPECT(h.game.tunes_completed() == 1);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1);
  EXPECT(h.game.follow_key() == -1);
  h.game.update(RainbowPianoGame::kCelebrateMs);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.tune_progress() == 0);
  h.follow_whole_tune();
  EXPECT(h.game.tunes_completed() == 2);
}

void test_off_light_keys_are_never_punished() {
  Harness h;
  h.enter_and_play();
  const std::size_t lit = static_cast<std::size_t>(h.game.follow_key());
  const std::size_t other = (lit + 1) % kKeyCount;
  for (int i = 0; i < 10; ++i) h.tap_key(other);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.tune_progress() == 0);
  EXPECT(h.fb.count_of(Cue::NOTE) == 10 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(h.game.follow_key() == static_cast<int>(lit));
}

void test_repeated_taps_and_input_during_celebration() {
  Harness h;
  h.enter_and_play();
  h.follow_whole_tune();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  const int notes = h.fb.count_of(Cue::NOTE);
  for (int i = 0; i < 20; ++i) h.tap_key(static_cast<std::size_t>(i) % kKeyCount);  // still plays
  h.say(Intent::AGAIN);
  h.tap_instrument(2);
  EXPECT(h.fb.count_of(Cue::NOTE) == notes + 21);
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.tunes_completed() == 1);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1);
  EXPECT(h.game.instrument() == Instrument::DRUM);
  h.game.update(RainbowPianoGame::kCelebrateMs);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.tune_progress() == 0);
}

void test_voice_again_matches_touch() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap_key(3);
  voice.tap_key(3);
  touch.tap_key(3);
  voice.say(Intent::AGAIN);
  EXPECT(touch.fb.count == voice.fb.count);
  for (int i = 0; i < touch.fb.count; ++i) {
    EXPECT(touch.fb.entries[i].cue == voice.fb.entries[i].cue);
    EXPECT(touch.fb.entries[i].variant == voice.fb.entries[i].variant);
  }
  EXPECT(touch.game.tune_progress() == voice.game.tune_progress());
  // With no key pressed yet, AGAIN plays the lit key, same as tapping it.
  Harness t2(11), v2(11);
  t2.enter_and_play();
  v2.enter_and_play();
  t2.tap_key(static_cast<std::size_t>(t2.game.follow_key()));
  v2.say(Intent::AGAIN);
  EXPECT(t2.fb.last()->variant == v2.fb.last()->variant);
  EXPECT(t2.game.tune_progress() == 1 && v2.game.tune_progress() == 1);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  const int before = h.fb.count;
  h.game.handle(make_touch_event(20, 80, h.t += 200));    // left of instruments
  h.game.handle(make_touch_event(64, 150, h.t += 200));   // gap between keys
  h.game.handle(make_touch_event(160, 30, h.t += 200));   // top bar
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.say(Intent::HELP);
  EXPECT(h.fb.count == before);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.notes_played() == 0);
}

void test_audio_disabled_still_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  RainbowPianoGame g;
  g.enter(quiet);
  g.update(RainbowPianoGame::kIntroMs);
  for (std::size_t i = 0; i < RainbowPianoGame::kTuneLength; ++i) {
    const std::size_t k = static_cast<std::size_t>(g.follow_key());
    const Rect& r = RainbowPianoGame::kKeys[k];
    g.handle(make_touch_event(static_cast<int16_t>(r.x + 5), static_cast<int16_t>(r.y + 5), 0));
    EXPECT(g.key_lit(k));  // visual counterpart of the note
  }
  EXPECT(g.phase() == Phase::CELEBRATE && g.tunes_completed() == 1);
  // Distinct shapes per key so keys are told apart without colour or audio.
  for (std::size_t i = 0; i < kKeyCount; ++i) {
    for (std::size_t j = i + 1; j < kKeyCount; ++j) EXPECT(shape_for_key(i) != shape_for_key(j));
  }
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.tap_instrument(1);
  h.follow_whole_tune();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();
  EXPECT(!h.game.active());
  const int before = h.fb.count;
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::AGAIN, 1));
  h.game.handle(make_touch_event(30, 150, 2));
  EXPECT(h.fb.count == before);
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.tunes_completed() == 0);
  EXPECT(h.game.instrument() == Instrument::BELLS && h.game.notes_played() == 0);
  EXPECT(h.game.last_key() == -1 && h.game.tune_progress() == 0);
  for (std::size_t k = 0; k < kKeyCount; ++k) EXPECT(!h.game.key_lit(k));
}

void test_same_seed_is_deterministic_and_seeds_vary() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int round = 0; round < 4; ++round) {
    for (std::size_t i = 0; i < RainbowPianoGame::kTuneLength; ++i) EXPECT(a.game.tune_note(i) == b.game.tune_note(i));
    a.follow_whole_tune();
    b.follow_whole_tune();
    a.game.update(RainbowPianoGame::kCelebrateMs);
    b.game.update(RainbowPianoGame::kCelebrateMs);
  }
  bool differ = false;
  for (uint32_t seed = 8; seed < 30 && !differ; ++seed) {
    Harness c(seed);
    c.enter_and_play();
    for (std::size_t i = 0; i < RainbowPianoGame::kTuneLength; ++i) differ |= c.game.tune_note(i) != a.game.tune_note(i);
  }
  EXPECT(differ);
}

void test_through_scene_manager() {
  RainbowPianoGame game;
  GameEntry entries[] = {{GameId::RAINBOW_PIANO, Intent::MUSIC, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::MUSIC, 100));
  EXPECT(game.active());
  mgr.update(RainbowPianoGame::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  mgr.handle(make_voice_event(Intent::RED, 300));  // out of context: never reaches game
  EXPECT(fb.count_of(Cue::NOTE) == 0);
  const Rect& k = RainbowPianoGame::kKeys[4];
  mgr.handle(make_touch_event(static_cast<int16_t>(k.x + 10), static_cast<int16_t>(k.y + 10), 500));
  EXPECT(fb.count_of(Cue::NOTE) == 1 && game.last_key() == 4);
  mgr.handle(make_voice_event(Intent::AGAIN, 700));  // Global intent reaches the game
  EXPECT(fb.count_of(Cue::NOTE) == 2);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_key_tap_during_intro_plays_immediately();
  test_every_key_emits_note_synchronously();
  test_three_instruments_give_distinct_notes();
  test_follow_light_completes_tune_and_celebrates();
  test_off_light_keys_are_never_punished();
  test_repeated_taps_and_input_during_celebration();
  test_voice_again_matches_touch();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic_and_seeds_vary();
  test_through_scene_manager();
  return little_arcade::testing::finish("rainbow piano tests");
}
