#include "little_arcade/games/shape_garage.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::shape_garage;
using little_arcade::testing::RecordingFeedback;

namespace {

using G = ShapeGarageGame;

Rect grow(const Rect& r, int16_t by) {
  return Rect{static_cast<int16_t>(r.x - by), static_cast<int16_t>(r.y - by), static_cast<int16_t>(r.w + 2 * by),
              static_cast<int16_t>(r.h + 2 * by)};
}

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  G game;
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
  void tap_piece(int i) { tap(game.piece(static_cast<std::size_t>(i)).rect); }
  void tap_slot(int i) { tap(game.slot(static_cast<std::size_t>(i)).rect); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  int slot_for(Shape s) const {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      if (game.slot(i).shape == s) return static_cast<int>(i);
    }
    return -1;
  }
  int wrong_slot_for(Shape s) const {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      if (game.slot(i).shape != s && !game.slot(i).filled) return static_cast<int>(i);
    }
    return -1;
  }
  int first_unplaced() const {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      if (!game.piece(i).placed) return static_cast<int>(i);
    }
    return -1;
  }
  // Tap-to-place every piece correctly.
  void repair_by_touch() {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      const int p = static_cast<int>(i);
      tap_piece(p);
      tap_slot(slot_for(game.piece(i).shape));
    }
  }
  void finish_celebration() { game.update(G::kCelebrateMs); }
};

void test_layout_is_toddler_safe() {
  Rect all[2 * G::kPieceCount + 1];
  std::size_t n = 0;
  for (const Rect& r : G::kSlotRects) all[n++] = r;
  for (const Rect& r : G::kTrayRects) all[n++] = r;
  for (std::size_t i = 0; i < n; ++i) {
    const Rect& a = all[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(a.y >= kPlayArea.y);  // primary targets in play area
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton) && !a.overlaps(G::kRobotHead));
    for (std::size_t j = i + 1; j < n; ++j) EXPECT(!grow(a, kMinSpacing).overlaps(all[j]));
  }
  // Forgiving slot hit areas stay on screen and never steal another target.
  for (std::size_t i = 0; i < G::kPieceCount; ++i) {
    const Rect hit = G::slot_hit_area(G::kSlotRects[i]);
    EXPECT(hit.on_screen() && hit.y >= kPlayArea.y);
    EXPECT(!hit.overlaps(kHomeButton) && !hit.overlaps(kMicButton) && !hit.overlaps(G::kRobotHead));
    for (std::size_t j = 0; j < G::kPieceCount; ++j) {
      EXPECT(!hit.overlaps(G::kTrayRects[j]));
      if (j != i) EXPECT(!hit.overlaps(G::slot_hit_area(G::kSlotRects[j])));
    }
  }
  const Rect& head = G::kRobotHead;
  EXPECT(head.on_screen() && head.large_enough());
  EXPECT(head.x >= 64 && head.x + head.w <= 256);  // optional top-bar control zone
  EXPECT(!grow(head, kMinSpacing).overlaps(kHomeButton) && !grow(head, kMinSpacing).overlaps(kMicButton));
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(G::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == kPromptFixRobot);
}

void test_intro_tap_skips_without_moving() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap_piece(0);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.selected() == -1 && h.game.filled_count() == 0);
}

void test_three_distinct_shapes_every_round() {
  Harness h;
  h.enter_and_play();
  for (int round = 0; round < 12; ++round) {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      EXPECT(!h.game.slot(i).filled && !h.game.piece(i).placed);
      EXPECT(h.slot_for(h.game.piece(i).shape) >= 0);  // every piece has a home
      for (std::size_t j = i + 1; j < G::kPieceCount; ++j) {
        EXPECT(h.game.slot(i).shape != h.game.slot(j).shape);
        EXPECT(h.game.piece(i).shape != h.game.piece(j).shape);
      }
    }
    EXPECT(h.game.robot_style() == round % G::kRobotStyles);
    h.repair_by_touch();
    EXPECT(h.game.phase() == Phase::CELEBRATE);
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY);
  }
  EXPECT(h.game.robots_repaired() == 12);
}

void test_tap_to_place_flow() {
  Harness h;
  h.enter_and_play();
  h.tap_piece(1);
  EXPECT(h.game.selected() == 1);
  EXPECT(h.fb.last()->cue == Cue::TAP);
  const int home = h.slot_for(h.game.piece(1).shape);
  h.tap_slot(home);
  EXPECT(h.game.piece(1).placed && h.game.slot(static_cast<std::size_t>(home)).filled);
  EXPECT(h.game.selected() == -1 && h.game.snapping() == home);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1);
  h.game.update(G::kSnapMs);
  EXPECT(h.game.snapping() == -1);
  // Placed pieces leave the tray: tapping their old spot does nothing.
  h.tap_piece(1);
  EXPECT(h.game.selected() == -1);
  // Switching the lifted piece is allowed.
  int other[2] = {-1, -1};
  int k = 0;
  for (int i = 0; i < 3; ++i) {
    if (i != 1) other[k++] = i;
  }
  h.tap_piece(other[0]);
  h.tap_piece(other[1]);
  EXPECT(h.game.selected() == other[1]);
}

void test_slot_edge_tap_is_forgiving() {
  Harness h;
  h.enter_and_play();
  h.tap_piece(0);
  const Rect& r = h.game.slot(static_cast<std::size_t>(h.slot_for(h.game.piece(0).shape))).rect;
  // Just outside the drawn edge, inside the forgiving area.
  h.game.handle(make_touch_event(static_cast<int16_t>(r.x - G::kSlotForgiveness + 1),
                                 static_cast<int16_t>(r.y + r.h + G::kSlotForgiveness - 1), h.t += 200));
  EXPECT(h.game.piece(0).placed);
}

void test_slot_first_lifts_matching_piece() {
  Harness h;
  h.enter_and_play();
  h.tap_slot(2);
  const int sel = h.game.selected();
  EXPECT(sel >= 0 && h.game.piece(static_cast<std::size_t>(sel)).shape == h.game.slot(2).shape);
  EXPECT(h.game.hint_slot() == 2 && h.game.hint_piece() == sel);
  h.tap_slot(2);
  EXPECT(h.game.slot(2).filled && h.game.piece(static_cast<std::size_t>(sel)).placed);
}

void test_wrong_slot_is_gentle_then_assisted() {
  Harness h;
  h.enter_and_play();
  h.tap_piece(0);
  const Shape s = h.game.piece(0).shape;
  const int wrong = h.wrong_slot_for(s);
  const int home = h.slot_for(s);
  h.tap_slot(wrong);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.selected() == 0);  // still holding the piece
  EXPECT(h.game.wiggling() == 0 && h.game.hint_slot() == home);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 1);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == prompt_for(s));
  EXPECT(!h.game.slot(static_cast<std::size_t>(wrong)).filled && h.game.filled_count() == 0);
  h.game.update(G::kHintMs);
  EXPECT(h.game.wiggling() == -1 && h.game.hint_slot() == -1);
  // Second miss: the piece slides into its own slot by itself.
  h.tap_slot(wrong);
  EXPECT(h.game.piece(0).placed && h.game.slot(static_cast<std::size_t>(home)).filled);
  EXPECT(!h.game.slot(static_cast<std::size_t>(wrong)).filled);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 1 && h.fb.count_of(Cue::SUCCESS) == 1);
}

void test_put_back_touch_and_voice() {
  Harness h;
  h.enter_and_play();
  h.tap_piece(2);
  h.tap_piece(2);  // tap again = put back (touch alternative to NO)
  EXPECT(h.game.selected() == -1);
  h.tap_piece(2);
  h.say(Intent::NO);
  EXPECT(h.game.selected() == -1 && h.game.filled_count() == 0);
  h.say(Intent::NO);  // nothing held: harmless
  EXPECT(h.game.selected() == -1);
}

void test_voice_and_touch_equivalent() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.repair_by_touch();
  for (int i = 0; i < 6; ++i) voice.say(Intent::YES);  // lift, place x3
  EXPECT(touch.game.phase() == Phase::CELEBRATE && voice.game.phase() == Phase::CELEBRATE);
  EXPECT(touch.game.robots_repaired() == 1 && voice.game.robots_repaired() == 1);
  EXPECT(touch.fb.count_of(Cue::SUCCESS) == 3 && voice.fb.count_of(Cue::SUCCESS) == 3);
  EXPECT(touch.fb.count_of(Cue::CELEBRATE) == 1 && voice.fb.count_of(Cue::CELEBRATE) == 1);
  touch.finish_celebration();
  voice.finish_celebration();
  for (std::size_t i = 0; i < G::kPieceCount; ++i) {
    EXPECT(touch.game.slot(i).shape == voice.game.slot(i).shape);
    EXPECT(touch.game.piece(i).shape == voice.game.piece(i).shape);
  }
}

void test_yes_matches_tap_sequence() {
  Harness touch(5), voice(5);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap_piece(touch.first_unplaced());
  voice.say(Intent::YES);
  EXPECT(touch.game.selected() == voice.game.selected());
  const int p = touch.game.selected();
  touch.tap_slot(touch.slot_for(touch.game.piece(static_cast<std::size_t>(p)).shape));
  voice.say(Intent::YES);
  for (std::size_t i = 0; i < G::kPieceCount; ++i) {
    EXPECT(touch.game.slot(i).filled == voice.game.slot(i).filled);
    EXPECT(touch.game.piece(i).placed == voice.game.piece(i).placed);
  }
}

void test_repeated_input_during_celebration() {
  Harness h;
  h.enter_and_play();
  h.repair_by_touch();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  const int success = h.fb.count_of(Cue::SUCCESS);
  for (int i = 0; i < 5; ++i) {
    h.tap_piece(0);
    h.tap_slot(0);
    h.say(Intent::YES);
    h.say(Intent::AGAIN);
  }
  EXPECT(h.game.robots_repaired() == 1 && h.fb.count_of(Cue::SUCCESS) == success);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  h.finish_celebration();
  EXPECT(h.game.phase() == Phase::PLAY && h.game.filled_count() == 0);
}

void test_repeated_taps_in_play() {
  Harness h;
  h.enter_and_play();
  for (int i = 0; i < 4; ++i) h.tap_piece(1);  // lift/put back toggles; never breaks
  EXPECT(h.game.selected() == -1);
  h.tap_piece(1);
  const int home = h.slot_for(h.game.piece(1).shape);
  h.tap_slot(home);
  h.tap_slot(home);  // filled slot: harmless
  h.tap_slot(home);
  EXPECT(h.game.filled_count() == 1 && h.fb.count_of(Cue::SUCCESS) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_voice_during_snap_animation() {
  Harness h;
  h.enter_and_play();
  h.say(Intent::YES);
  h.say(Intent::YES);
  EXPECT(h.game.snapping() >= 0);
  h.say(Intent::YES);  // mid-snap: lifts the next piece normally
  h.say(Intent::AGAIN);
  EXPECT(h.game.selected() >= 0 && h.game.hint_slot() >= 0);
  EXPECT(h.game.filled_count() == 1);
}

void test_again_help_and_head_replay_prompt() {
  Harness h;
  h.enter_and_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.say(Intent::AGAIN);
  EXPECT(h.fb.last()->variant == kPromptFixRobot && h.game.hint_piece() >= 0);
  h.tap_piece(0);
  h.say(Intent::HELP);
  EXPECT(h.fb.last()->variant == prompt_for(h.game.piece(0).shape));
  EXPECT(h.game.hint_slot() == h.slot_for(h.game.piece(0).shape));
  h.tap(G::kRobotHead);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 3);
  EXPECT(h.game.selected() == 0 && h.game.filled_count() == 0);
}

void test_empty_taps_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(10, 200, h.t += 200));   // left margin
  h.game.handle(make_touch_event(160, 150, h.t += 200));  // gap between rows
  h.game.handle(make_touch_event(300, 120, h.t += 200));  // right margin
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.say(Intent::THREE);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.filled_count() == 0 && h.game.selected() == -1);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0 && h.fb.count_of(Cue::TAP) == 0);
}

void test_audio_disabled_still_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  G g;
  g.enter(quiet);
  g.update(G::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY);
  for (std::size_t i = 0; i < G::kPieceCount; ++i) {
    const Rect& pr = g.piece(i).rect;
    g.handle(make_touch_event(static_cast<int16_t>(pr.x + 5), static_cast<int16_t>(pr.y + 5), 0));
    EXPECT(g.selected() == static_cast<int>(i));  // visual lift
    for (std::size_t s = 0; s < G::kPieceCount; ++s) {
      if (g.slot(s).shape == g.piece(i).shape) {
        const Rect& sr = g.slot(s).rect;
        g.handle(make_touch_event(static_cast<int16_t>(sr.x + 5), static_cast<int16_t>(sr.y + 5), 0));
      }
    }
  }
  EXPECT(g.phase() == Phase::CELEBRATE && g.robots_repaired() == 1);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.tap_piece(0);
  h.tap_slot(h.wrong_slot_for(h.game.piece(0).shape));  // wiggle + hint running
  h.game.exit();
  EXPECT(!h.game.active());
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::YES, 1));
  h.game.handle(make_touch_event(150, 100, 2));
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.filled_count() == 0 && h.game.selected() == -1);
  EXPECT(h.game.wiggling() == -1 && h.game.hint_slot() == -1 && h.game.robots_repaired() == 0);

  // Exit during the celebration too.
  h.game.update(G::kIntroMs);
  h.repair_by_touch();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();
  h.game.update(G::kCelebrateMs);
  EXPECT(h.game.phase() == Phase::INTRO && !h.game.active());
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.robots_repaired() == 0 && h.game.robot_style() == 0);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int round = 0; round < 5; ++round) {
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      EXPECT(a.game.slot(i).shape == b.game.slot(i).shape);
      EXPECT(a.game.piece(i).shape == b.game.piece(i).shape);
    }
    a.repair_by_touch();
    b.repair_by_touch();
    a.finish_celebration();
    b.finish_celebration();
  }
}

void test_layouts_vary_between_seeds() {
  bool differ = false;
  Harness first(1);
  first.enter_and_play();
  for (uint32_t seed = 2; seed < 20 && !differ; ++seed) {
    Harness other(seed);
    other.enter_and_play();
    for (std::size_t i = 0; i < G::kPieceCount; ++i) {
      differ |= first.game.slot(i).shape != other.game.slot(i).shape;
      differ |= first.game.piece(i).shape != other.game.piece(i).shape;
    }
  }
  EXPECT(differ);
}

void test_through_scene_manager() {
  G game;
  GameEntry entries[] = {{GameId::SHAPE_GARAGE, Intent::SHAPES, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::SHAPES, 100));
  EXPECT(game.active() && mgr.mode() == SceneManager::Mode::PLAYING);
  mgr.update(G::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  mgr.handle(make_voice_event(Intent::GO, 300));  // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::RED, 400));
  EXPECT(game.selected() == -1 && fb.count_of(Cue::TAP) == 0);
  uint32_t t = 500;
  for (int i = 0; i < 6; ++i) mgr.handle(make_voice_event(Intent::YES, t += 200));
  EXPECT(game.robots_repaired() == 1);
  mgr.update(G::kCelebrateMs);
  const Rect& r = game.piece(0).rect;
  mgr.handle(make_touch_event(static_cast<int16_t>(r.x + 10), static_cast<int16_t>(r.y + 10), t += 200));
  EXPECT(game.selected() == 0);
  mgr.handle(make_touch_event(10, 10, t += 200));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_tap_skips_without_moving();
  test_three_distinct_shapes_every_round();
  test_tap_to_place_flow();
  test_slot_edge_tap_is_forgiving();
  test_slot_first_lifts_matching_piece();
  test_wrong_slot_is_gentle_then_assisted();
  test_put_back_touch_and_voice();
  test_voice_and_touch_equivalent();
  test_yes_matches_tap_sequence();
  test_repeated_input_during_celebration();
  test_repeated_taps_in_play();
  test_voice_during_snap_animation();
  test_again_help_and_head_replay_prompt();
  test_empty_taps_and_unrelated_intents_are_harmless();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_layouts_vary_between_seeds();
  test_through_scene_manager();
  return little_arcade::testing::finish("shape garage tests");
}
