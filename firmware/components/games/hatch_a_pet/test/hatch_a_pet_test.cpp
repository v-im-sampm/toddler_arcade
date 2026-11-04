#include <initializer_list>

#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::hatch_a_pet;
using little_arcade::testing::RecordingFeedback;

namespace {

using G = HatchAPetGame;
static_assert(kCreatureCount >= 6, "spec: at least 6 original creatures");

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  HatchAPetGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(G::kIntroMs);
  }
  void tap(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2),
                                 t += 200));
  }
  void tap_at(int16_t x, int16_t y) { game.handle(make_touch_event(x, y, t += 200)); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void crack_open() {
    while (game.phase() == Phase::EGG) tap(G::kEgg);
  }
  void finish_hatch() { game.update(G::kHatchMs); }
  void hatch_to_pet() {
    crack_open();
    finish_hatch();
  }
};

Rect grown(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void expect_safe(const Rect& r) {
  EXPECT(r.on_screen() && r.large_enough());
  EXPECT(!r.overlaps(kHomeButton) && !r.overlaps(kMicButton));
}

void test_layout_is_toddler_safe() {
  // Primary targets live in the play area.
  for (const Rect& r : {G::kEgg, G::kCreature, G::kNoButton, G::kYesButton}) {
    expect_safe(r);
    EXPECT(r.y >= kPlayArea.y);
    EXPECT(!r.overlaps(G::kPromptBadge));
  }
  // Optional top-bar badge stays between Home and Mic.
  expect_safe(G::kPromptBadge);
  EXPECT(G::kPromptBadge.x >= 64 && G::kPromptBadge.x + G::kPromptBadge.w <= 256);
  // Targets shown together (PET: creature + two buttons) are spaced apart.
  EXPECT(!grown(G::kCreature).overlaps(G::kNoButton));
  EXPECT(!grown(G::kCreature).overlaps(G::kYesButton));
  EXPECT(!grown(G::kNoButton).overlaps(G::kYesButton));
  // The egg (EGG phase) also stays clear of where the buttons appear.
  EXPECT(!grown(G::kEgg).overlaps(G::kNoButton));
  EXPECT(!grown(G::kEgg).overlaps(G::kYesButton));
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(G::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::EGG);
  EXPECT(h.fb.last()->cue == Cue::PROMPT &&
         h.fb.last()->variant == static_cast<uint8_t>(Prompt::TAP_THE_EGG));
}

void test_intro_tap_skips_without_cracking() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap(G::kEgg);
  EXPECT(h.game.phase() == Phase::EGG);
  EXPECT(h.game.cracks() == 0);
}

void test_three_to_five_taps_crack_the_egg() {
  bool saw[G::kMaxTaps + 1] = {};
  for (uint32_t seed = 1; seed < 40; ++seed) {
    Harness h(seed);
    h.enter_and_play();
    for (int egg = 0; egg < 4; ++egg) {
      const uint8_t need = h.game.taps_needed();
      EXPECT(need >= G::kMinTaps && need <= G::kMaxTaps);
      if (need <= G::kMaxTaps) saw[need] = true;
      for (uint8_t i = 1; i < need; ++i) {
        h.tap(G::kEgg);
        EXPECT(h.game.phase() == Phase::EGG && h.game.cracks() == i);  // every tap progresses
        EXPECT(h.game.wobbling());
      }
      h.tap(G::kEgg);
      EXPECT(h.game.phase() == Phase::HATCH);
      h.finish_hatch();
      EXPECT(h.game.phase() == Phase::PET);
      h.tap(G::kYesButton);
      EXPECT(h.game.phase() == Phase::EGG && h.game.cracks() == 0);
    }
  }
  EXPECT(saw[3] && saw[4] && saw[5]);
}

void test_cues_have_expected_variants() {
  Harness h;
  h.enter_and_play();
  h.fb.clear();
  h.crack_open();
  EXPECT(h.fb.count_of(Cue::TAP) == h.game.taps_needed());
  EXPECT(h.fb.count_of(Cue::NOTE) == h.game.taps_needed());
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1 && h.fb.count_of(Cue::CELEBRATE) == 1);
  h.fb.clear();
  h.finish_hatch();
  EXPECT(h.fb.count_of(Cue::REACTION) == 1);
  EXPECT(h.fb.entries[0].variant == static_cast<uint8_t>(h.game.creature()));
  EXPECT(h.fb.last()->cue == Cue::PROMPT &&
         h.fb.last()->variant == static_cast<uint8_t>(Prompt::ANOTHER_EGG));
  EXPECT(h.game.reacting());
}

void test_at_least_six_creatures_and_variety() {
  Harness h;
  h.enter_and_play();
  bool seen[kCreatureCount] = {};
  Creature previous = h.game.creature();
  for (std::size_t i = 0; i < kCreatureCount; ++i) {
    EXPECT(static_cast<std::size_t>(h.game.creature()) < kCreatureCount);
    seen[static_cast<std::size_t>(h.game.creature())] = true;
    h.hatch_to_pet();
    h.say(Intent::AGAIN);
    EXPECT(h.game.creature() != previous);
    previous = h.game.creature();
  }
  std::size_t distinct = 0;
  for (bool s : seen) distinct += s ? 1u : 0u;
  EXPECT(kCreatureCount >= 6);
  EXPECT(distinct == kCreatureCount);  // a full bag shows every creature
  // No back-to-back repeats over a long run, including across bag refills.
  for (int i = 0; i < 40; ++i) {
    const Creature before = h.game.creature();
    h.hatch_to_pet();
    h.tap(G::kYesButton);
    EXPECT(h.game.creature() != before);
  }
  EXPECT(h.game.hatch_count() == kCreatureCount + 40);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int egg = 0; egg < 10; ++egg) {
    EXPECT(a.game.creature() == b.game.creature());
    EXPECT(a.game.egg_pattern() == b.game.egg_pattern());
    EXPECT(a.game.taps_needed() == b.game.taps_needed());
    a.crack_open();
    while (b.game.phase() == Phase::EGG) b.say(Intent::YES);
    a.finish_hatch();
    b.finish_hatch();
    a.tap(G::kYesButton);
    b.say(Intent::YES);
  }
  bool differs = false;
  Harness first(1);
  first.enter_and_play();
  for (uint32_t seed = 2; seed < 20 && !differs; ++seed) {
    Harness other(seed);
    other.enter_and_play();
    differs = other.game.creature() != first.game.creature() ||
              other.game.taps_needed() != first.game.taps_needed();
  }
  EXPECT(differs);
}

void test_voice_and_touch_equivalent() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  // YES <-> tap egg
  touch.tap(G::kEgg);
  voice.say(Intent::YES);
  EXPECT(touch.game.cracks() == 1 && voice.game.cracks() == 1);
  touch.crack_open();
  while (voice.game.phase() == Phase::EGG) voice.say(Intent::YES);
  EXPECT(touch.game.phase() == Phase::HATCH && voice.game.phase() == Phase::HATCH);
  touch.finish_hatch();
  voice.finish_hatch();
  // NO <-> NO button (stay and pet)
  touch.tap(G::kNoButton);
  voice.say(Intent::NO);
  EXPECT(touch.game.phase() == Phase::PET && voice.game.phase() == Phase::PET);
  EXPECT(touch.game.pet_count() == 1 && voice.game.pet_count() == 1);
  // YES <-> YES button (another egg)
  touch.tap(G::kYesButton);
  voice.say(Intent::YES);
  EXPECT(touch.game.phase() == Phase::EGG && voice.game.phase() == Phase::EGG);
  EXPECT(touch.game.creature() == voice.game.creature());
  // AGAIN/HELP <-> prompt badge
  const int before_t = touch.fb.count_of(Cue::PROMPT);
  const int before_v = voice.fb.count_of(Cue::PROMPT);
  touch.tap(G::kPromptBadge);
  voice.say(Intent::AGAIN);
  EXPECT(touch.fb.count_of(Cue::PROMPT) == before_t + 1 && voice.fb.count_of(Cue::PROMPT) == before_v + 1);
  EXPECT(touch.game.hint_active() && voice.game.hint_active());
  EXPECT(touch.game.cracks() == 0 && voice.game.cracks() == 0);
}

void test_petting_and_again_in_pet() {
  Harness h;
  h.enter_and_play();
  h.hatch_to_pet();
  const Creature pet = h.game.creature();
  for (int i = 0; i < 5; ++i) h.tap(G::kCreature);
  h.tap_at(300, 220);  // empty corner still makes the pet react
  EXPECT(h.game.pet_count() == 6 && h.game.phase() == Phase::PET && h.game.creature() == pet);
  h.say(Intent::HELP);
  EXPECT(h.game.phase() == Phase::PET && h.game.hint_active());
  h.say(Intent::AGAIN);
  EXPECT(h.game.phase() == Phase::EGG);
  h.hatch_to_pet();
  h.say(Intent::NEXT);
  EXPECT(h.game.phase() == Phase::EGG);
}

void test_no_and_unrelated_input_in_egg_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.say(Intent::NO);
  h.say(Intent::NEXT);
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.tap_at(100, 30);  // top bar, not the badge
  EXPECT(h.game.phase() == Phase::EGG && h.game.cracks() == 0);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
  h.tap_at(20, 220);  // anywhere in the play area still cracks the egg
  EXPECT(h.game.cracks() == 1);
}

void test_repeated_and_voice_input_during_hatch() {
  Harness h;
  h.enter_and_play();
  h.crack_open();
  EXPECT(h.game.phase() == Phase::HATCH);
  const uint8_t cracks = h.game.cracks();
  h.fb.clear();
  for (int i = 0; i < 10; ++i) h.tap(G::kEgg);
  h.say(Intent::YES);
  h.say(Intent::NO);
  h.say(Intent::AGAIN);
  h.tap(G::kYesButton);
  EXPECT(h.game.phase() == Phase::HATCH && h.game.cracks() == cracks);
  EXPECT(h.game.hatch_count() == 1 && h.fb.count == 0);
  h.finish_hatch();
  EXPECT(h.game.phase() == Phase::PET);
}

void test_audio_disabled_still_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  HatchAPetGame g;
  g.enter(quiet);
  g.update(G::kIntroMs);
  EXPECT(g.phase() == Phase::EGG);
  const Rect& e = G::kEgg;
  uint32_t t = 0;
  while (g.phase() == Phase::EGG) {
    g.handle(make_touch_event(static_cast<int16_t>(e.x + 10), static_cast<int16_t>(e.y + 10), t += 200));
    EXPECT(g.wobbling());  // visual counterpart of TAP/NOTE
  }
  g.update(G::kHatchMs);
  EXPECT(g.phase() == Phase::PET && g.reacting() && g.hatch_count() == 1);
  EXPECT(g.current_prompt() == Prompt::ANOTHER_EGG);  // badge shows the question as a picture
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.crack_open();
  EXPECT(h.game.phase() == Phase::HATCH);
  h.game.exit();  // Home pressed mid-hatch
  EXPECT(!h.game.active());
  const int cues = h.fb.count;
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::YES, 1));
  h.game.handle(make_touch_event(160, 120, 2));
  EXPECT(h.fb.count == cues);
  h.game.enter(h.ctx);  // fresh start, no saved profile
  EXPECT(h.game.phase() == Phase::INTRO && h.game.hatch_count() == 0 && h.game.cracks() == 0);
  EXPECT(h.game.pet_count() == 0 && !h.game.reacting());
}

void test_through_scene_manager() {
  HatchAPetGame game;
  GameEntry entries[] = {{GameId::HATCH_A_PET, Intent::EGGS, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::EGGS, 100));
  EXPECT(game.active());
  EXPECT(mgr.active_contexts() == (Context::General | Context::Global));
  mgr.update(G::kIntroMs);
  EXPECT(game.phase() == Phase::EGG);
  mgr.handle(make_voice_event(Intent::GO, 300));  // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::EGGS, 400));
  EXPECT(game.cracks() == 0);
  uint32_t t = 500;
  while (game.phase() == Phase::EGG) mgr.handle(make_voice_event(Intent::YES, t += 200));
  EXPECT(game.phase() == Phase::HATCH);
  mgr.update(G::kHatchMs);
  mgr.handle(make_touch_event(160, 120, t += 200));  // pet via touch
  EXPECT(game.pet_count() == 1);
  mgr.handle(make_touch_event(10, 10, t += 200));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_tap_skips_without_cracking();
  test_three_to_five_taps_crack_the_egg();
  test_cues_have_expected_variants();
  test_at_least_six_creatures_and_variety();
  test_same_seed_is_deterministic();
  test_voice_and_touch_equivalent();
  test_petting_and_again_in_pet();
  test_no_and_unrelated_input_in_egg_are_harmless();
  test_repeated_and_voice_input_during_hatch();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_through_scene_manager();
  return little_arcade::testing::finish("hatch a pet tests");
}
