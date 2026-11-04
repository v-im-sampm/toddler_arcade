#include "little_arcade/games/silly_faces.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::silly_faces;
using little_arcade::testing::RecordingFeedback;

namespace {

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  SillyFacesGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(SillyFacesGame::kIntroMs);
  }
  void tap(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2),
                                 t += 200));
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void finish_celebration() { game.update(SillyFacesGame::kCelebrateMs); }
  // Tap eyes until they match the mouth (at most 3 taps on a mixed face).
  int match_by_eyes() {
    int taps = 0;
    while (game.phase() == Phase::PLAY && !game.face().matched() && taps < 8) {
      tap(SillyFacesGame::kEyesZone);
      ++taps;
    }
    return taps;
  }
};

Rect grown(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void test_layout_is_toddler_safe() {
  for (std::size_t i = 0; i < SillyFacesGame::kTargetCount; ++i) {
    const Rect& a = SillyFacesGame::kTargets[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton));
    for (std::size_t j = i + 1; j < SillyFacesGame::kTargetCount; ++j) {
      EXPECT(!grown(a).overlaps(SillyFacesGame::kTargets[j]));
    }
  }
  // Primary play targets live in the play area; the badge is in the top bar between Home and Mic.
  for (std::size_t i = 0; i + 1 < SillyFacesGame::kTargetCount; ++i) {
    const Rect& a = SillyFacesGame::kTargets[i];
    EXPECT(a.y >= kPlayArea.y);
  }
  const Rect& badge = SillyFacesGame::kPromptBadge;
  EXPECT(badge.x >= 64 && badge.x + badge.w <= 256);
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(!h.game.face().matched());  // starts as a silly mixed face
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(SillyFacesGame::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT);
  EXPECT(h.game.hint_active());  // visual twin of the prompt
}

void test_intro_input_skips_without_changing_face() {
  Harness h;
  h.game.enter(h.ctx);
  const Face before = h.game.face();
  h.tap(SillyFacesGame::kEyesZone);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.face().eyes == before.eyes && h.game.face().mouth == before.mouth);
  Harness v;
  v.game.enter(v.ctx);
  v.say(Intent::YES);
  EXPECT(v.game.phase() == Phase::PLAY && v.game.faces_made() == 0);
}

void test_eyes_and_mouth_cycle_through_four_emotions() {
  Harness h;
  h.enter_and_play();
  // Make the face mixed-and-cycle the mouth: every tap advances by one, wrapping after 4.
  const Emotion start_mouth = h.game.face().mouth;
  int celebrations = 0;
  for (int i = 0; i < 4; ++i) {
    h.tap(SillyFacesGame::kMouthZone);
    EXPECT(h.game.bouncing() == Part::MOUTH);
    if (h.game.phase() == Phase::CELEBRATE) {
      ++celebrations;
      h.finish_celebration();
    }
  }
  EXPECT(h.game.face().mouth == start_mouth);
  EXPECT(celebrations == 1);  // exactly one mouth style matches the eyes
  EXPECT(h.fb.count_of(Cue::TAP) == 4);
}

void test_all_four_emotions_can_be_made() {
  Harness h;
  h.enter_and_play();
  for (int round = 0; round < 4; ++round) {
    // Set eyes to the target emotion by cycling, then match the mouth.
    const Emotion want = static_cast<Emotion>(round);
    int guard = 0;
    while (h.game.face().eyes != want && guard++ < 8) {
      h.tap(SillyFacesGame::kEyesZone);
      if (h.game.phase() == Phase::CELEBRATE && h.game.face().eyes != want) h.finish_celebration();
    }
    guard = 0;
    while (!(h.game.face().matched() && h.game.face().eyes == want) && guard++ < 8) {
      h.tap(SillyFacesGame::kMouthZone);
      if (h.game.phase() == Phase::CELEBRATE && h.game.face().eyes != want) h.finish_celebration();
    }
    EXPECT(h.game.phase() == Phase::CELEBRATE);
    EXPECT(h.game.showing() == want && !h.game.showing_silly());
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY);
  }
  EXPECT(h.game.emotions_made() == SillyFacesGame::kAllEmotionsMask);
  // Each emotion has its own reaction variant; the full set earns one big celebration.
  for (uint8_t e = 0; e < kEmotionCount; ++e) {
    bool seen = false;
    for (int i = 0; i < h.fb.count; ++i) {
      seen |= h.fb.entries[i].cue == Cue::REACTION && h.fb.entries[i].variant == e;
    }
    EXPECT(seen);
  }
  int big = 0;
  for (int i = 0; i < h.fb.count; ++i) {
    big += h.fb.entries[i].cue == Cue::CELEBRATE && h.fb.entries[i].variant == kCelebrateAllFour ? 1 : 0;
  }
  EXPECT(big == 1);
}

void test_no_grading_ever() {
  Harness h;
  h.enter_and_play();
  // Show a mixed face: it giggles (positive), nothing is judged.
  EXPECT(!h.game.face().matched());
  h.tap(SillyFacesGame::kShowButton);
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.showing_silly());
  EXPECT(h.fb.last()->cue == Cue::REACTION && h.fb.last()->variant == kSillyReaction);
  h.finish_celebration();
  // Hammer every control and intent; never a TRY_AGAIN, never stuck.
  const Rect* rects = SillyFacesGame::kTargets;
  for (int i = 0; i < 60; ++i) {
    h.tap(rects[i % SillyFacesGame::kTargetCount]);
    h.say(i % 2 ? Intent::NO : Intent::YES);
    h.game.update(SillyFacesGame::kCelebrateMs);
  }
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_mix_and_new_monster() {
  Harness h;
  h.enter_and_play();
  for (int i = 0; i < 20; ++i) {
    const Face before = h.game.face();
    h.tap(SillyFacesGame::kMixButton);
    EXPECT(h.game.phase() == Phase::PLAY);
    EXPECT(!h.game.face().matched());
    EXPECT(h.game.face().eyes != before.eyes || h.game.face().mouth != before.mouth);
    EXPECT(h.game.bouncing() == Part::WHOLE);
  }
  for (int i = 0; i < 20; ++i) {
    const uint8_t color = h.game.body_color();
    h.tap(SillyFacesGame::kNewMonsterButton);
    EXPECT(h.game.body_color() != color && h.game.body_color() < kBodyColorCount);
    EXPECT(static_cast<std::size_t>(h.game.topper()) < kTopperCount);
    EXPECT(!h.game.face().matched());
  }
}

void test_voice_and_touch_equivalent() {
  struct Pair {
    Rect rect;
    Intent intent;
  };
  const Pair pairs[] = {{SillyFacesGame::kShowButton, Intent::YES},
                        {SillyFacesGame::kMixButton, Intent::NO},
                        {SillyFacesGame::kNewMonsterButton, Intent::NEXT},
                        {SillyFacesGame::kPromptBadge, Intent::AGAIN},
                        {SillyFacesGame::kPromptBadge, Intent::HELP}};
  for (const Pair& p : pairs) {
    Harness touch(9), voice(9);
    touch.enter_and_play();
    voice.enter_and_play();
    touch.tap(p.rect);
    voice.say(p.intent);
    EXPECT(touch.game.phase() == voice.game.phase());
    EXPECT(touch.game.face().eyes == voice.game.face().eyes);
    EXPECT(touch.game.face().mouth == voice.game.face().mouth);
    EXPECT(touch.game.body_color() == voice.game.body_color());
    EXPECT(touch.game.hint_active() == voice.game.hint_active());
    EXPECT(touch.fb.count == voice.fb.count);
    EXPECT(touch.fb.last()->cue == voice.fb.last()->cue && touch.fb.last()->variant == voice.fb.last()->variant);
  }
}

void test_repeated_taps_and_voice_during_celebration() {
  Harness h;
  h.enter_and_play();
  h.match_by_eyes();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  const Face shown = h.game.face();
  const int cues = h.fb.count;
  for (int i = 0; i < 10; ++i) {
    h.tap(SillyFacesGame::kEyesZone);
    h.tap(SillyFacesGame::kMixButton);
    h.say(Intent::YES);
    h.say(Intent::NO);
    h.say(Intent::NEXT);
  }
  EXPECT(h.fb.count == cues);  // ignored while acting out
  EXPECT(h.game.face().eyes == shown.eyes && h.game.face().mouth == shown.mouth);
  EXPECT(h.game.faces_made() == 1);
  h.finish_celebration();
  // Rapid taps in PLAY just keep cycling; each one is handled, none get stuck.
  for (int i = 0; i < 12; ++i) {
    h.tap(SillyFacesGame::kMouthZone);
    if (h.game.phase() == Phase::CELEBRATE) h.finish_celebration();
  }
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  const Face before = h.game.face();
  const int cues = h.fb.count;
  h.game.handle(make_touch_event(240, 200, h.t += 200));  // gap beside mouth
  h.game.handle(make_touch_event(80, 100, h.t += 200));   // gap between show and eyes
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.say(Intent::UNKNOWN);
  EXPECT(h.fb.count == cues);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.face().eyes == before.eyes && h.game.face().mouth == before.mouth);
}

void test_idle_hint_is_visual_only() {
  Harness h;
  h.enter_and_play();
  h.game.update(SillyFacesGame::kHintMs);
  EXPECT(!h.game.hint_active());
  const int cues = h.fb.count;
  h.game.update(SillyFacesGame::kIdleHintMs);
  EXPECT(h.game.hint_active());
  EXPECT(h.fb.count == cues);
  EXPECT(h.game.phase() == Phase::PLAY);  // nothing lost by waiting
}

void test_audio_disabled_still_playable() {
  Harness h;
  EXPECT(!h.ctx.audio_enabled);
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  SillyFacesGame g;
  g.enter(quiet);
  g.update(SillyFacesGame::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY && g.hint_active());
  int guard = 0;
  while (g.phase() == Phase::PLAY && guard++ < 8) {
    const Rect& r = SillyFacesGame::kEyesZone;
    g.handle(make_touch_event(static_cast<int16_t>(r.x + 5), static_cast<int16_t>(r.y + 5),
                              static_cast<uint32_t>(guard) * 200u));
  }
  EXPECT(g.phase() == Phase::CELEBRATE && g.faces_made() == 1);  // visible act-out
  g.update(SillyFacesGame::kCelebrateMs);
  EXPECT(g.phase() == Phase::PLAY);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.match_by_eyes();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();  // Home pressed mid-celebration
  EXPECT(!h.game.active());
  EXPECT(h.game.bouncing() == Part::NONE && !h.game.hint_active());
  const int cues = h.fb.count;
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::YES, 1));
  h.game.handle(make_touch_event(150, 100, 2));
  EXPECT(h.fb.count == cues);
  h.game.enter(h.ctx);  // fresh start, no saved profile
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.faces_made() == 0 && h.game.emotions_made() == 0);
  EXPECT(!h.game.face().matched());
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int i = 0; i < 10; ++i) {
    EXPECT(a.game.face().eyes == b.game.face().eyes && a.game.face().mouth == b.game.face().mouth);
    EXPECT(a.game.body_color() == b.game.body_color() && a.game.topper() == b.game.topper());
    a.tap(SillyFacesGame::kNewMonsterButton);
    b.say(Intent::NEXT);
    a.say(Intent::NO);
    b.tap(SillyFacesGame::kMixButton);
  }
  bool differs = false;
  Harness first(1);
  first.game.enter(first.ctx);
  for (uint32_t seed = 2; seed < 20 && !differs; ++seed) {
    Harness other(seed);
    other.game.enter(other.ctx);
    differs = other.game.face().eyes != first.game.face().eyes ||
              other.game.face().mouth != first.game.face().mouth ||
              other.game.body_color() != first.game.body_color();
  }
  EXPECT(differs);
}

void test_through_scene_manager() {
  SillyFacesGame game;
  GameEntry entries[] = {{GameId::SILLY_FACES, Intent::FACES, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::FACES, 100));
  EXPECT(game.active());
  EXPECT((mgr.active_contexts() & static_cast<uint32_t>(Context::General)) != 0);
  mgr.update(SillyFacesGame::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  const int cues = fb.count;
  mgr.handle(make_voice_event(Intent::GO, 300));   // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::RED, 350));  // out of context
  EXPECT(fb.count == cues);
  mgr.handle(make_voice_event(Intent::YES, 500));
  EXPECT(game.phase() == Phase::CELEBRATE);
  mgr.update(SillyFacesGame::kCelebrateMs);
  const Rect& eyes = SillyFacesGame::kEyesZone;
  mgr.handle(make_touch_event(static_cast<int16_t>(eyes.x + 10), static_cast<int16_t>(eyes.y + 10), 900));
  EXPECT(game.bouncing() == Part::EYES || game.phase() == Phase::CELEBRATE);
  mgr.handle(make_touch_event(10, 10, 1500));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_input_skips_without_changing_face();
  test_eyes_and_mouth_cycle_through_four_emotions();
  test_all_four_emotions_can_be_made();
  test_no_grading_ever();
  test_mix_and_new_monster();
  test_voice_and_touch_equivalent();
  test_repeated_taps_and_voice_during_celebration();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_idle_hint_is_visual_only();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_through_scene_manager();
  return little_arcade::testing::finish("silly faces tests");
}
