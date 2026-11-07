#include <cmath>
#include <cstring>

#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::hatch_a_pet;
using little_arcade::testing::RecordingFeedback;
using G = HatchAPetGame;

namespace {
constexpr double kDt = 1.0 / 60;

struct Harness {
  Rng rng;
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  HatchAPetGame game;
  uint32_t ms = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) { game.enter(ctx); }
  void run(double seconds) {
    for (int i = 0, n = static_cast<int>(std::lround(seconds / kDt)); i < n; ++i) {
      game.step(kDt);
      game.frame_drawn();
    }
  }
  void tap(double x, double y) {
    game.handle(make_touch_event(static_cast<int16_t>(std::lround(x)), static_cast<int16_t>(std::lround(y)), ms += 100));
    run(0.1);  // past the tap debounce
  }
  void say(Intent intent) {
    game.handle(make_voice_event(intent, ms += 100));
    run(0.1);
  }
  void to_egg() {
    run(1.25);
  }
  void knock_until_pet() {
    while (game.phase() == Phase::EGG) tap(G::kEggX, G::kEggY);
    run(0.45);  // pre-hatch shake
    run(1.65);  // hatch
  }
  void to_choices() {
    while (!game.choices().show) {
      tap(game.pet().x, game.pet().y);
      run(0.6);
    }
    run(0.4);  // choices accept input after 0.3 s
  }
};

void test_intro_then_egg() {
  Harness h;
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.fb.count_of(Cue::AMBIENT) == 1);
  EXPECT(h.game.egg().y_off == -200);
  h.run(0.75);
  EXPECT(h.game.phase() == Phase::INTRO && std::fabs(h.game.egg().y_off) < 1e-9);
  EXPECT(h.fb.last() != nullptr);
  h.run(0.5);
  EXPECT(h.game.phase() == Phase::EGG);
  EXPECT(h.game.egg().need >= G::kMinTaps && h.game.egg().need <= G::kMaxTaps);
}

void test_start_overlay() {
  Harness h;
  h.game.reset_to_start();
  EXPECT(h.game.phase() == Phase::START);
  h.run(2);
  EXPECT(h.game.phase() == Phase::START);
  h.tap(5, 230);  // anywhere starts
  EXPECT(h.game.phase() == Phase::INTRO);
  h.tap(G::kEggX, G::kEggY);  // intro is skippable
  EXPECT(h.game.phase() == Phase::EGG);
}

void test_knocks_crack_and_hatch() {
  for (uint32_t seed = 1; seed < 40; ++seed) {
    Harness h(seed);
    h.to_egg();
    const int need = h.game.egg().need;
    EXPECT(need >= 3 && need <= 5);
    int taps = 0;
    h.fb.clear();
    while (h.game.phase() == Phase::EGG && taps < 10) {
      h.tap(G::kEggX, G::kEggY);
      ++taps;
    }
    EXPECT(taps == need);
    EXPECT(h.game.egg().cracks == need);
    EXPECT(h.game.phase() == Phase::PRE_HATCH);
    h.run(0.45);
    EXPECT(h.game.phase() == Phase::HATCH);
    EXPECT(h.fb.count_of(Cue::SUCCESS) == 1);
    EXPECT(h.game.hatch_count() == 1);
    h.tap(G::kEggX, G::kEggY);  // ignored while hatching
    EXPECT(h.game.phase() == Phase::HATCH);
    h.run(1.6);
    EXPECT(h.game.phase() == Phase::PET);
  }
}

void test_debounce() {
  Harness h;
  h.to_egg();
  h.game.handle(make_touch_event(160, 138, 1));
  h.game.handle(make_touch_event(160, 138, 2));  // same instant: debounced
  EXPECT(h.game.egg().cracks == 1);
}

void test_reactions_cycle_then_choices() {
  Harness h;
  h.to_egg();
  h.knock_until_pet();
  h.run(0.8);  // name chirp jump finishes
  EXPECT(h.game.pet().react < 0);
  int seen[3] = {0, 0, 0};
  for (int i = 0; i < 3; ++i) {
    h.fb.clear();
    h.tap(h.game.pet().x, h.game.pet().y);
    EXPECT(h.game.pet().react == i);
    EXPECT(h.fb.count_of(Cue::REACTION) == 1);
    seen[i]++;
    h.run(0.9);
  }
  EXPECT(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
  EXPECT(h.game.choices().show);
}

void test_choices_after_timeout() {
  Harness h;
  h.to_egg();
  h.knock_until_pet();
  h.run(4.5);  // TOK.choiceWait = 5 s
  EXPECT(!h.game.choices().show);
  h.run(0.6);
  EXPECT(h.game.choices().show);
}

void test_no_keeps_pet_with_hearts() {
  Harness h;
  h.to_egg();
  h.knock_until_pet();
  h.to_choices();
  h.tap(G::kNoX, G::kNoY);
  EXPECT(h.game.phase() == Phase::PET);
  EXPECT(!h.game.choices().show);
  EXPECT(h.game.pet().reacts == 0);
  bool heart = false;
  for (std::size_t i = 0; i < h.game.particle_count(); ++i) heart = heart || h.game.particle(i).shape == ParticleShape::HEART;
  EXPECT(heart);
  EXPECT(h.game.shelf_count() == 0);
}

void test_yes_goes_to_shelf_and_rolls_new_egg() {
  Harness h;
  h.to_egg();
  h.knock_until_pet();
  h.to_choices();
  const Creature c = h.game.egg().creature;
  h.tap(G::kYesX, G::kYesY);
  EXPECT(h.game.phase() == Phase::BYE);
  h.run(0.95);
  EXPECT(h.game.phase() == Phase::ROLL);
  EXPECT(h.game.shelf_count() == 1 && h.game.shelf(0).c == c);
  EXPECT(h.game.egg().creature != c);
  h.run(0.95);
  EXPECT(h.game.phase() == Phase::EGG);
  EXPECT(h.game.egg().cracks == 0);
}

void test_shelf_full_shifts_oldest_out() {
  Harness h(7);
  h.to_egg();
  Creature order[10];
  for (int i = 0; i < 8; ++i) {
    order[i] = h.game.egg().creature;
    if (i > 0) EXPECT(order[i] != order[i - 1]);
    h.knock_until_pet();
    h.to_choices();
    h.say(Intent::YES);
    h.run(2);
    EXPECT(h.game.phase() == Phase::EGG);
    EXPECT(h.game.shelf_count() == static_cast<std::size_t>(i + 1 < 6 ? i + 1 : 6));
  }
  // The last six friends, oldest first, glide to their slots.
  for (std::size_t i = 0; i < 6; ++i) {
    EXPECT(h.game.shelf(i).c == order[i + 2]);
    EXPECT(h.game.shelf(i).tx == G::kShelf[i][0]);
    EXPECT(std::fabs(h.game.shelf(i).x - G::kShelf[i][0]) < 1);
  }
  // Shuffle bag: the first eight hatches are all different creatures.
  bool used[kCreatureCount] = {};
  for (int i = 0; i < 8; ++i) {
    EXPECT(!used[static_cast<int>(order[i])]);
    used[static_cast<int>(order[i])] = true;
  }
}

void test_badge_tap() {
  Harness h;
  h.to_egg();
  h.fb.clear();
  h.tap(160, 30);
  EXPECT(h.game.badge().last_tap > 0);
  EXPECT(h.fb.last() && h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant == cue::kKnockKnock);
  EXPECT(h.game.egg().cracks == 0);
  h.knock_until_pet();
  h.run(0.8);
  h.fb.clear();
  h.tap(160, 30);
  EXPECT(h.fb.count_of(Cue::REACTION) == 1);
  EXPECT(h.game.pet().reacts == 0);  // badge giggle does not count toward choices
}

void test_idle_hints() {
  Harness h;
  h.to_egg();
  h.fb.clear();
  h.run(5.9);
  EXPECT(h.game.hint() == 0);
  h.run(0.2);
  EXPECT(h.game.hint() == 1);
  h.run(6);
  EXPECT(h.game.hint() == 2);
  int kk = 0;
  for (int i = 0; i < h.fb.count && i < RecordingFeedback::kCapacity; ++i) {
    kk += h.fb.entries[i].cue == Cue::NOTE && h.fb.entries[i].variant == cue::kKnockKnock;
  }
  EXPECT(kk == 1);
  h.tap(G::kEggX, G::kEggY);
  EXPECT(h.game.hint() == 0);
}

void test_voice_matches_touch() {
  Harness touch(11), voice(11);
  touch.to_egg();
  voice.to_egg();
  EXPECT(touch.game.egg().need == voice.game.egg().need);
  while (touch.game.phase() == Phase::EGG) {
    touch.tap(G::kEggX, G::kEggY);
    voice.say(Intent::YES);
  }
  EXPECT(voice.game.phase() == touch.game.phase());
  touch.run(2.1);
  voice.run(2.1);
  EXPECT(voice.game.phase() == Phase::PET);
  voice.say(Intent::NEXT);
  EXPECT(voice.game.pet().reacts == 1);
}

void test_seeded_runs_are_deterministic() {
  Harness a(3), b(3);
  a.game.seed(1234, 99);
  b.game.seed(1234, 99);
  a.game.reset_to_start();
  b.game.reset_to_start();
  for (int i = 0; i < 600; ++i) {
    if (i % 30 == 5) {
      a.game.pointer_down(160, 138);
      b.game.pointer_down(160, 138);
    }
    a.game.step(kDt);
    b.game.step(kDt);
    a.game.frame_drawn();
    b.game.frame_drawn();
  }
  EXPECT(a.game.phase() == b.game.phase());
  EXPECT(a.game.particle_count() == b.game.particle_count());
  EXPECT(a.game.egg().creature == b.game.egg().creature);
  EXPECT(a.game.pet().x == b.game.pet().x);
  EXPECT(a.fb.count == b.fb.count);
}

void test_particles_capped() {
  Harness h;
  h.to_egg();
  for (int i = 0; i < 200; ++i) {
    h.game.pointer_down(300, 120);  // background sparkle taps
    h.game.step(kDt);
    EXPECT(h.game.particle_count() <= G::kMaxParticles);
    EXPECT(h.game.ring_count() <= G::kMaxRings);
  }
}

void test_through_scene_manager() {
  HatchAPetGame game;
  GameEntry entries[] = {{GameId::HATCH_A_PET, Intent::EGGS, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::EGGS, 100));
  EXPECT(game.active());
  EXPECT(game.phase() == Phase::INTRO);
  EXPECT(mgr.active_contexts() == (Context::General | Context::Global));
  for (int i = 0; i < 80; ++i) mgr.update(16);
  EXPECT(game.phase() == Phase::EGG);
  mgr.handle(make_voice_event(Intent::GO, 300));  // out of context: never reaches the game
  EXPECT(game.egg().cracks == 0);
  uint32_t t = 500;
  while (game.phase() == Phase::EGG) {
    mgr.handle(make_touch_event(160, 138, t += 200));
    for (int i = 0; i < 6; ++i) mgr.update(16);
  }
  EXPECT(game.phase() == Phase::PRE_HATCH);
  for (int i = 0; i < 140; ++i) mgr.update(16);
  EXPECT(game.phase() == Phase::PET);
  mgr.handle(make_touch_event(10, 10, t += 200));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_intro_then_egg();
  test_start_overlay();
  test_knocks_crack_and_hatch();
  test_debounce();
  test_reactions_cycle_then_choices();
  test_choices_after_timeout();
  test_no_keeps_pet_with_hearts();
  test_yes_goes_to_shelf_and_rolls_new_egg();
  test_shelf_full_shifts_oldest_out();
  test_badge_tap();
  test_idle_hints();
  test_voice_matches_touch();
  test_seeded_runs_are_deterministic();
  test_particles_capped();
  test_through_scene_manager();
  return testing::finish("hatch_a_pet_test");
}
