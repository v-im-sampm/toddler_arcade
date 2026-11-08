#include "../model.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>

using namespace hatch_a_pet;

namespace {
int failures = 0;
#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
      ++failures;                                                               \
    }                                                                           \
  } while (0)

void down(Model& model, double x, double y) {
  model.pointer_down(x, y);
  model.pointer_up();
}

void run(Model& model, double seconds, double dt = 1.0 / 60.0) {
  while (seconds > 1e-10) {
    const double step = seconds < dt ? seconds : dt;
    model.tick(step);
    seconds -= step;
  }
}

void enter_egg(Model& model) {
  down(model, 160, 120);
  run(model, 1.2);
  CHECK(model.state().phase == Phase::Egg);
  model.take_cues();
}

void hatch(Model& model) {
  const int need = model.state().egg.need;
  for (int i = 0; i < need; ++i) {
    down(model, 160, 138);
    if (i + 1 < need) run(model, 0.081);
  }
  CHECK(model.state().phase == Phase::PreHatch);
  run(model, 0.42);  // Leave one host tick of tolerance for accumulated doubles.
  CHECK(model.state().phase == Phase::Hatch);
  CHECK(model.state().shell.visible);
  run(model, 1.62);
  CHECK(model.state().phase == Phase::Pet);
}

void show_choice_by_reactions(Model& model) {
  const Reaction expected[] = {Reaction::Giggle, Reaction::Jump, Reaction::Sneeze};
  for (int i = 0; i < 3; ++i) {
    down(model, model.state().pet.x, model.state().pet.y);
    CHECK(model.state().pet.reaction == expected[i]);
    run(model, 0.081);
  }
  run(model, 0.001);
  run(model, 0.31);
  CHECK(model.state().choice.visible);
}

bool last_cue_is(Model& model, CueId cue) {
  const auto cues = model.take_cues();
  CHECK(!cues.empty());
  return !cues.empty() && cues.back().id == cue;
}

void test_rng_and_seeded_bag() {
  constexpr Creature expected_creatures[] = {
      Creature::Bloop, Creature::Puddlepup, Creature::Glimmerbug, Creature::RolyPole,
      Creature::Snorkle, Creature::Zigzag, Creature::Sproutling, Creature::Fluffwing
  };
  constexpr Pattern expected_patterns[] = {
      Pattern::Stars, Pattern::Spots, Pattern::Stripes, Pattern::Zigzags,
      Pattern::Stripes, Pattern::Spots, Pattern::Stars, Pattern::Stars
  };
  constexpr int expected_needs[] = {4, 4, 5, 3, 4, 5, 4, 5};
  Mulberry32 rng(1234);
  CHECK(std::fabs(rng.next() - 0.07329497812315822) < 1e-15);
  Model model(1234);
  enter_egg(model);
  CHECK(model.state().egg.creature == expected_creatures[0]);

  std::set<int> first_bag;
  first_bag.insert(static_cast<int>(model.state().egg.creature));
  for (int i = 0; i < 8; ++i) {
    CHECK(model.state().egg.creature == expected_creatures[i]);
    CHECK(model.state().egg.pattern == expected_patterns[i]);
    CHECK(model.state().egg.need == expected_needs[i]);
    if (i == 7) break;
    hatch(model);
    show_choice_by_reactions(model);
    down(model, 272, 168);
    run(model, 0.92);
    run(model, 0.92);
    CHECK(model.state().phase == Phase::Egg);
    first_bag.insert(static_cast<int>(model.state().egg.creature));
    model.take_cues();
  }
  CHECK(first_bag.size() == 8);
  const Creature previous = model.state().egg.creature;
  hatch(model);
  show_choice_by_reactions(model);
  down(model, 272, 168);
  run(model, 0.92);
  run(model, 0.92);
  CHECK(model.state().egg.creature != previous);  // Shuffle-bag boundary cannot repeat.
  CHECK(model.state().egg.creature == Creature::Bloop);
  CHECK(model.state().egg.pattern == Pattern::Stripes);
  CHECK(model.state().egg.need == 3);
}

void test_intro_feedback_knocks_and_locks() {
  Model model(19);
  CHECK(model.state().phase == Phase::Start);
  model.pointer_down(4, 200);  // Anywhere starts, matching the reference.
  CHECK(model.state().phase == Phase::Intro);
  CHECK(model.state().touch.serial == 1);
  CHECK(model.state().touch.down);
  CHECK(model.state().touch.target == TouchTarget::Start);
  model.pointer_up();
  CHECK(model.state().touch.release_serial == 1);
  CHECK(model.state().touch.released_target == TouchTarget::Start);

  run(model, 0.7);
  CHECK(model.state().phase == Phase::Intro);
  CHECK(model.state().egg.landed);
  CHECK(model.state().egg.y_offset == 0);
  run(model, 0.5);
  CHECK(model.state().phase == Phase::Egg);
  CHECK(model.state().prompt_seconds == 0.2);
  run(model, 0.22);  // Timer expires on the first host tick at/after 200 ms.
  CHECK(last_cue_is(model, CueId::PromptKnock));

  const int need = model.state().egg.need;
  down(model, 160, 138);
  CHECK(model.state().egg.cracks == 1);
  CHECK(model.state().touch.target == TouchTarget::Egg);
  auto cues = model.take_cues();
  CHECK(cues.size() == 1 && cues[0].id == CueId::Knock && cues[0].variant == 1);
  down(model, 160, 138);  // 80 ms debounce is strict and does not double-knock.
  CHECK(model.state().egg.cracks == 1);
  CHECK(model.take_cues().empty());
  for (int i = 1; i < need; ++i) {
    run(model, 0.081);
    down(model, 160, 138);
  }
  CHECK(model.state().phase == Phase::PreHatch);
  CHECK(model.state().egg.cracks == need);
  CHECK(last_cue_is(model, CueId::PreHatchTrill));
  const uint64_t serial = model.state().touch.serial;
  down(model, 160, 138);
  CHECK(model.state().touch.serial == serial + 1);
  CHECK(model.state().touch.target == TouchTarget::Blocked);
  CHECK(model.state().egg.cracks == need);  // Hatch transition taps do not stack sounds/actions.
  CHECK(model.take_cues().empty());
}

void test_allocation_free_cue_drain() {
  Model model(12);
  down(model, 160, 120);
  CueEvent cue{};
  CHECK(model.pop_cue(cue));
  CHECK(cue.id == CueId::MusicStart);
  CHECK(!model.pop_cue(cue));
}

void test_empty_taps_hints_and_commands() {
  Model model(42);
  enter_egg(model);
  down(model, 240, 80);
  CHECK(model.state().touch.target == TouchTarget::Empty);
  auto cues = model.take_cues();
  CHECK(cues.size() == 1 && cues[0].id == CueId::EmptyTap);
  run(model, 6.01);
  CHECK(model.state().hint == 1);
  CHECK(model.command(Command::Help));
  CHECK(model.state().egg.wobble_seconds > 0);
  CHECK(last_cue_is(model, CueId::EggHint));
  run(model, 12.01);
  CHECK(model.state().hint == 2);
  CHECK(model.state().spoken_hint);
  CHECK(last_cue_is(model, CueId::PromptKnock));

  Model skip(9);
  down(skip, 160, 120);
  down(skip, 240, 100);
  CHECK(skip.state().phase == Phase::Intro);
  run(skip, 1.0);
  CHECK(skip.state().phase == Phase::Egg);
}

void test_pet_choice_no_yes_shelf_and_home() {
  Model model(77);
  enter_egg(model);
  hatch(model);
  CHECK(model.state().hatches == 1);
  CHECK(!model.state().shell.visible);
  model.take_cues();
  show_choice_by_reactions(model);
  auto cues = model.take_cues();
  CHECK(cues.back().id == CueId::ChoiceAppear);
  down(model, 48, 168);
  CHECK(model.state().phase == Phase::Pet);
  CHECK(!model.state().choice.visible);
  CHECK(model.state().choice.selected == Decision::Stay);
  CHECK(model.state().pet.reactions == 0);
  cues = model.take_cues();
  CHECK(!cues.empty() && cues.front().id == CueId::ChoiceNo);

  show_choice_by_reactions(model);
  run(model, 0.31);
  CHECK(model.command(Command::Yes));
  CHECK(model.state().phase == Phase::Bye);
  CHECK(model.state().choice.selected == Decision::AnotherEgg);
  run(model, 0.92);
  CHECK(model.state().phase == Phase::Roll);
  CHECK(model.state().shelf.size() == 1);
  CHECK(model.state().shelf[0].x == 24);
  run(model, 0.92);
  CHECK(model.state().phase == Phase::Egg);
  CHECK(model.state().egg.x == 160);
  down(model, 24, 221);
  CHECK(model.state().touch.target == TouchTarget::Shelf);
  CHECK(model.state().shelf[0].wave_seconds > 0.5);
  CHECK(last_cue_is(model, CueId::ShelfWave));
  model.home();
  CHECK(model.state().phase == Phase::Start);
  CHECK(model.state().shelf.empty());
  CHECK(model.state().choice.fade_seconds == 0);
}

void test_shelf_capacity_evicts_oldest() {
  Model model(208);
  enter_egg(model);
  Creature oldest = model.state().egg.creature;
  for (int i = 0; i < 7; ++i) {
    hatch(model);
    show_choice_by_reactions(model);
    down(model, 272, 168);
    run(model, 0.92);
    run(model, 0.92);
    CHECK(model.state().phase == Phase::Egg);
    if (i == 0) CHECK(model.state().shelf.size() == 1);
    if (i == 6) {
      CHECK(model.state().shelf.size() == 6);
      CHECK(model.state().shelf[0].creature != oldest);
    }
  }
}

void test_choice_timeout_debounce_and_yes_boundary() {
  Model model(83);
  enter_egg(model);
  hatch(model);
  run(model, 4.9);
  CHECK(!model.state().choice.visible);
  run(model, 0.12);
  CHECK(model.state().choice.visible);
  CHECK(model.state().choice.elapsed > 0);
  CHECK(!model.command(Command::Yes));  // Choice touch/voice lock is 300 ms.
  down(model, 272, 168);
  CHECK(model.state().phase == Phase::Pet);
  run(model, 0.31);
  CHECK(model.command(Command::Yes));
  CHECK(model.state().phase == Phase::Bye);
}

void test_cosmetic_rng_is_independent() {
  Model a(101), b(101);
  enter_egg(a);
  enter_egg(b);
  run(a, 20);
  run(b, 20, 0.011);
  a.home();
  b.home();
  down(a, 160, 120);
  down(b, 160, 120);
  CHECK(a.state().egg.creature == b.state().egg.creature);
  CHECK(a.state().egg.pattern == b.state().egg.pattern);
  CHECK(a.state().egg.need == b.state().egg.need);
}
}  // namespace

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  test_rng_and_seeded_bag();
  test_intro_feedback_knocks_and_locks();
  test_allocation_free_cue_drain();
  test_empty_taps_hints_and_commands();
  test_pet_choice_no_yes_shelf_and_home();
  test_shelf_capacity_evicts_oldest();
  test_choice_timeout_debounce_and_yes_boundary();
  test_cosmetic_rng_is_independent();
  if (failures) {
    std::printf("%d Hatch-a-Pet model checks failed\n", failures);
    return EXIT_FAILURE;
  }
  std::puts("Hatch-a-Pet deterministic gameplay and reference edge cases OK");
  return EXIT_SUCCESS;
}
