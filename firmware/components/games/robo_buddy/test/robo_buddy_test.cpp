#include "little_arcade/games/robo_buddy.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::robo_buddy;
using little_arcade::testing::RecordingFeedback;

namespace {

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  RoboBuddyGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(RoboBuddyGame::kIntroMs);
  }
  void tap(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2),
                                 static_cast<int16_t>(r.y + r.h / 2), t += 200));
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void finish_reaction() { game.update(RoboBuddyGame::kDanceMs); }
  void finish_doze() { game.update(RoboBuddyGame::kDozeMs); }
};

Rect grow(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void test_layout_is_toddler_safe() {
  for (std::size_t i = 0; i < RoboBuddyGame::kTargetCount; ++i) {
    const Rect& a = RoboBuddyGame::kTargets[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(!grow(a).overlaps(kHomeButton) && !grow(a).overlaps(kMicButton));
    for (std::size_t j = i + 1; j < RoboBuddyGame::kTargetCount; ++j) {
      EXPECT(!grow(a).overlaps(RoboBuddyGame::kTargets[j]));
    }
  }
  // Primary targets in the play area; the hello button is an optional top-bar control.
  const Rect primary[] = {RoboBuddyGame::kHead, RoboBuddyGame::kBelly, RoboBuddyGame::kFoodButton,
                          RoboBuddyGame::kSleepButton};
  for (const Rect& r : primary) EXPECT(r.y >= kPlayArea.y);
  const Rect& hello = RoboBuddyGame::kHelloButton;
  EXPECT(hello.x >= 64 && hello.x + hello.w <= 256);
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.mood() == Mood::CALM);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(RoboBuddyGame::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT);
}

void test_intro_tap_skips_without_acting() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap(RoboBuddyGame::kHead);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.reactions() == 0);
  EXPECT(h.fb.count_of(Cue::REACTION) == 0);
}

void test_three_distinct_moods() {
  Harness h;
  h.enter_and_play();
  h.tap(RoboBuddyGame::kHead);
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.mood() == Mood::HAPPY);
  EXPECT(h.fb.last()->cue == Cue::NOTE);
  h.finish_reaction();
  h.tap(RoboBuddyGame::kBelly);
  EXPECT(h.game.mood() == Mood::SILLY);
  h.finish_reaction();
  h.tap(RoboBuddyGame::kFoodButton);
  EXPECT(h.game.mood() == Mood::YUMMY);
  h.finish_reaction();
  h.tap(RoboBuddyGame::kSleepButton);
  EXPECT(h.game.mood() == Mood::SLEEPY);
  int distinct = 0;
  for (std::size_t m = 0; m < kMoodCount; ++m) distinct += (h.game.moods_seen_mask() >> m) & 1u ? 1 : 0;
  EXPECT(distinct == 5);
  EXPECT(h.fb.count_of(Cue::REACTION) == 4);
}

void test_sleep_and_wake() {
  Harness h;
  h.enter_and_play();
  h.tap(RoboBuddyGame::kSleepButton);
  EXPECT(h.game.sleeping() && h.game.dozing() && h.game.naps() == 1);
  EXPECT(h.fb.any(Cue::AMBIENT));
  h.tap(RoboBuddyGame::kHead);  // during the yawn: ignored
  EXPECT(h.game.sleeping());
  h.finish_doze();
  EXPECT(h.game.sleeping() && !h.game.dozing());
  h.game.update(60000);  // stays asleep, nothing bad happens
  EXPECT(h.game.sleeping() && h.game.mood() == Mood::SLEEPY);
  h.game.handle(make_touch_event(300, 70, h.t += 200));  // empty area does not wake
  EXPECT(h.game.sleeping());
  h.tap(RoboBuddyGame::kHelloButton);  // hint, still asleep
  EXPECT(h.game.sleeping() && h.game.hint_active());
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == 1);
  h.tap(RoboBuddyGame::kSleepButton);  // sleep button toggles to wake
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.last_action() == Action::WAKE);
  EXPECT(h.game.mood() == Mood::HAPPY);
  h.finish_reaction();
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_voice_wakes_robot() {
  const Intent wakers[] = {Intent::YES, Intent::NO, Intent::AGAIN};
  for (Intent i : wakers) {
    Harness h;
    h.enter_and_play();
    h.tap(RoboBuddyGame::kSleepButton);
    h.finish_doze();
    h.say(Intent::HELP);
    EXPECT(h.game.sleeping() && h.game.hint_active());
    h.say(i);
    EXPECT(!h.game.sleeping() && h.game.last_action() == Action::WAKE);
  }
}

void test_mood_relaxes_when_idle() {
  Harness h;
  h.enter_and_play();
  h.tap(RoboBuddyGame::kBelly);
  h.finish_reaction();
  EXPECT(h.game.mood() == Mood::SILLY);
  h.game.update(RoboBuddyGame::kCalmAfterMs);
  EXPECT(h.game.mood() == Mood::CALM && h.game.phase() == Phase::PLAY);
}

void test_voice_and_touch_equivalent() {
  struct Pair {
    Rect rect;
    Intent intent;
    Mood mood;
  };
  const Pair pairs[] = {{RoboBuddyGame::kHead, Intent::YES, Mood::HAPPY},
                        {RoboBuddyGame::kBelly, Intent::NO, Mood::SILLY}};
  for (const Pair& p : pairs) {
    Harness touch(9), voice(9);
    touch.enter_and_play();
    voice.enter_and_play();
    touch.tap(p.rect);
    voice.say(p.intent);
    EXPECT(touch.game.mood() == p.mood && voice.game.mood() == p.mood);
    EXPECT(touch.game.pose() == voice.game.pose());
    EXPECT(touch.game.reactions() == 1 && voice.game.reactions() == 1);
  }
  // AGAIN repeats the last action; touch alternative is tapping the same part again.
  Harness touch(5), voice(5);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap(RoboBuddyGame::kFoodButton);
  voice.tap(RoboBuddyGame::kFoodButton);
  touch.finish_reaction();
  voice.finish_reaction();
  touch.tap(RoboBuddyGame::kFoodButton);
  voice.say(Intent::AGAIN);
  EXPECT(touch.game.mood() == Mood::YUMMY && voice.game.mood() == Mood::YUMMY);
  EXPECT(touch.game.snack() == voice.game.snack() && touch.game.reactions() == 2);
  // AGAIN with no history pets the robot.
  Harness fresh;
  fresh.enter_and_play();
  fresh.say(Intent::AGAIN);
  EXPECT(fresh.game.mood() == Mood::HAPPY);
  // AGAIN after a sleep naps again (touch alternative: sleep button).
  Harness nap;
  nap.enter_and_play();
  nap.tap(RoboBuddyGame::kSleepButton);
  nap.finish_doze();
  nap.tap(RoboBuddyGame::kSleepButton);
  nap.finish_reaction();
  nap.say(Intent::AGAIN);  // last action is WAKE -> pet
  EXPECT(nap.game.mood() == Mood::HAPPY && !nap.game.sleeping());
}

void test_hello_button_matches_help() {
  Harness touch, voice;
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap(RoboBuddyGame::kHelloButton);
  voice.say(Intent::HELP);
  EXPECT(touch.game.hint_active() && voice.game.hint_active());
  EXPECT(touch.fb.count_of(Cue::PROMPT) == 2 && voice.fb.count_of(Cue::PROMPT) == 2);
  EXPECT(touch.game.reactions() == 0 && touch.game.phase() == Phase::PLAY);
}

void test_poses_vary_and_snack_changes() {
  Harness h;
  h.enter_and_play();
  uint8_t previous_pose = 255;
  bool seen[RoboBuddyGame::kPoseCount] = {};
  for (int i = 0; i < 12; ++i) {
    const Snack before = h.game.snack();
    h.tap(RoboBuddyGame::kFoodButton);
    EXPECT(h.game.snack() != before);
    EXPECT(h.game.pose() < RoboBuddyGame::kPoseCount && h.game.pose() != previous_pose);
    previous_pose = h.game.pose();
    seen[previous_pose] = true;
    h.finish_reaction();
  }
  EXPECT(seen[0] && seen[1] && seen[2]);
}

void test_dance_every_fifth_reaction() {
  Harness h;
  h.enter_and_play();
  for (uint32_t i = 1; i <= 10; ++i) {
    h.tap(i % 2 ? RoboBuddyGame::kHead : RoboBuddyGame::kBelly);
    EXPECT(h.game.dancing() == (i % RoboBuddyGame::kDanceEvery == 0));
    h.game.update(RoboBuddyGame::kReactMs);
    if (i % RoboBuddyGame::kDanceEvery == 0) {
      EXPECT(h.game.phase() == Phase::CELEBRATE);  // dance runs longer
      h.game.update(RoboBuddyGame::kDanceMs - RoboBuddyGame::kReactMs);
    }
    EXPECT(h.game.phase() == Phase::PLAY);
  }
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 2);
}

void test_repeated_input_during_reaction() {
  Harness h;
  h.enter_and_play();
  h.tap(RoboBuddyGame::kHead);
  for (int i = 0; i < 10; ++i) h.tap(RoboBuddyGame::kHead);
  h.say(Intent::YES);
  h.say(Intent::NO);
  h.tap(RoboBuddyGame::kSleepButton);
  EXPECT(h.game.reactions() == 1 && h.game.mood() == Mood::HAPPY && !h.game.sleeping());
  EXPECT(h.fb.count_of(Cue::REACTION) == 1 && h.fb.count_of(Cue::TAP) == 1);
  h.finish_reaction();
  for (int i = 0; i < 20; ++i) {  // mashing across reactions keeps working, never fails
    h.tap(RoboBuddyGame::kHead);
    h.game.update(100);
  }
  EXPECT(h.game.reactions() >= 2);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(100, 100, h.t += 200));  // gap left of head
  h.game.handle(make_touch_event(50, 225, h.t += 200));   // under food button
  h.say(Intent::GO);
  h.say(Intent::NEXT);
  h.say(Intent::RED);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.reactions() == 0);
  EXPECT(h.fb.count_of(Cue::TAP) == 0 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_audio_disabled_still_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  RoboBuddyGame g;
  g.enter(quiet);
  g.update(RoboBuddyGame::kIntroMs);
  g.handle(make_touch_event(160, 100, 100));
  EXPECT(g.mood() == Mood::HAPPY);
  g.update(RoboBuddyGame::kDanceMs);
  g.handle(make_touch_event(260, 160, 400));
  EXPECT(g.sleeping());
  g.update(RoboBuddyGame::kDozeMs);
  g.handle(make_touch_event(160, 180, 600));
  EXPECT(!g.sleeping() && g.mood() == Mood::HAPPY);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.tap(RoboBuddyGame::kFoodButton);
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();  // Home pressed mid-reaction
  EXPECT(!h.game.active());
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::YES, 1));
  h.game.handle(make_touch_event(160, 100, 2));
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.reactions() == 0 && h.game.mood() == Mood::CALM);
  EXPECT(h.game.last_action() == Action::NONE);
  // Exit while asleep, re-enter awake.
  h.game.update(RoboBuddyGame::kIntroMs);
  h.tap(RoboBuddyGame::kSleepButton);
  h.game.exit();
  h.game.enter(h.ctx);
  EXPECT(!h.game.sleeping() && h.game.naps() == 0);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  EXPECT(a.game.snack() == b.game.snack());
  const Rect seq[] = {RoboBuddyGame::kFoodButton, RoboBuddyGame::kHead, RoboBuddyGame::kBelly,
                      RoboBuddyGame::kFoodButton, RoboBuddyGame::kHead, RoboBuddyGame::kFoodButton};
  for (const Rect& r : seq) {
    a.tap(r);
    b.tap(r);
    EXPECT(a.game.pose() == b.game.pose() && a.game.snack() == b.game.snack());
    EXPECT(a.game.mood() == b.game.mood());
    a.finish_reaction();
    b.finish_reaction();
  }
  EXPECT(a.fb.count == b.fb.count);
  for (int i = 0; i < a.fb.count && i < RecordingFeedback::kCapacity; ++i) {
    EXPECT(a.fb.entries[i].cue == b.fb.entries[i].cue && a.fb.entries[i].variant == b.fb.entries[i].variant);
  }
}

void test_through_scene_manager() {
  RoboBuddyGame game;
  GameEntry entries[] = {{GameId::ROBO_BUDDY, Intent::ROBOT, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::ROBOT, 100));
  EXPECT(game.active());
  mgr.update(RoboBuddyGame::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  mgr.handle(make_voice_event(Intent::GO, 300));  // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::RED, 350));
  EXPECT(game.reactions() == 0 && fb.count_of(Cue::REACTION) == 0);
  mgr.handle(make_voice_event(Intent::YES, 500));
  EXPECT(game.reactions() == 1 && game.mood() == Mood::HAPPY);
  mgr.handle(make_touch_event(10, 10, 900));  // Home mid-reaction
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_tap_skips_without_acting();
  test_three_distinct_moods();
  test_sleep_and_wake();
  test_voice_wakes_robot();
  test_mood_relaxes_when_idle();
  test_voice_and_touch_equivalent();
  test_hello_button_matches_help();
  test_poses_vary_and_snack_changes();
  test_dance_every_fifth_reaction();
  test_repeated_input_during_reaction();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_through_scene_manager();
  return little_arcade::testing::finish("robo buddy tests");
}
