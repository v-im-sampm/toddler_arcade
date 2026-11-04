#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using little_arcade::testing::RecordingFeedback;

namespace {

constexpr uint32_t bit(Context c) { return static_cast<uint32_t>(c); }

class SpyGame final : public IGame {
 public:
  explicit SpyGame(uint32_t contexts) : contexts_(contexts) {}
  void enter(GameContext& context) override {
    ++entered;
    first_random = context.rng.next();
  }
  void update(uint32_t dt_ms) override { elapsed += dt_ms; }
  void handle(const InputEvent& e) override {
    ++handled;
    last = e;
  }
  void exit() override { ++exited; }
  uint32_t active_contexts() const override { return contexts_; }
  void on_pause() override { ++paused; }
  void on_resume() override { ++resumed; }

  int entered = 0, exited = 0, handled = 0, paused = 0, resumed = 0;
  uint32_t elapsed = 0, first_random = 0;
  InputEvent last{};

 private:
  uint32_t contexts_;
};

struct Fixture {
  SpyGame colors{bit(Context::Colors)};
  SpyGame eggs{bit(Context::General)};
  SpyGame music{0};
  SpyGame robot{bit(Context::General)};
  SpyGame space{bit(Context::Space)};
  GameEntry entries[5] = {
      {GameId::COLOR_POP, Intent::COLORS, &colors},  {GameId::HATCH_A_PET, Intent::EGGS, &eggs},
      {GameId::RAINBOW_PIANO, Intent::MUSIC, &music}, {GameId::ROBO_BUDDY, Intent::ROBOT, &robot},
      {GameId::ROCKET_RESCUE, Intent::SPACE, &space},
  };
  RecordingFeedback feedback;
  SceneManager mgr{entries, 5, feedback};
  uint32_t t = 1000;

  void tap(const Rect& r) {
    t += 200;
    mgr.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2),
                                static_cast<int16_t>(r.y + r.h / 2), t));
  }
  void say(Intent intent) {
    t += 200;
    mgr.handle(make_voice_event(intent, t));
  }
};

void test_layout_rules() {
  for (const Rect& r : SceneManager::kTileRects) {
    EXPECT(r.on_screen() && r.large_enough());
    EXPECT(!r.overlaps(kHomeButton) && !r.overlaps(kMicButton));
    EXPECT(!r.overlaps(SceneManager::kNextPageButton));
  }
  EXPECT(SceneManager::kNextPageButton.on_screen() && SceneManager::kNextPageButton.large_enough());
  EXPECT(kMicButton.on_screen() && kMicButton.large_enough());
  EXPECT(!kHomeButton.overlaps(kMicButton));
}

void test_paging_and_tile_launch() {
  Fixture f;
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU);
  EXPECT(f.mgr.page_count() == 2);
  EXPECT(f.mgr.tile(0)->id == GameId::COLOR_POP);
  f.tap(SceneManager::kNextPageButton);
  EXPECT(f.mgr.page() == 1);
  EXPECT(f.mgr.tile(0)->id == GameId::ROCKET_RESCUE);
  EXPECT(f.mgr.tile(1) == nullptr);
  f.tap(SceneManager::kTileRects[1]);  // empty slot: nothing happens
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU);
  f.say(Intent::NEXT);
  EXPECT(f.mgr.page() == 0);
  f.tap(SceneManager::kTileRects[2]);
  EXPECT(f.mgr.mode() == SceneManager::Mode::PLAYING);
  EXPECT(f.mgr.current()->id == GameId::RAINBOW_PIANO);
  EXPECT(f.music.entered == 1);
}

void test_voice_launch_and_start() {
  Fixture f;
  f.say(Intent::SPACE);
  EXPECT(f.mgr.current() && f.mgr.current()->id == GameId::ROCKET_RESCUE);
  f.say(Intent::BACK);
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU && f.space.exited == 1);
  f.say(Intent::START);
  EXPECT(f.mgr.current() && f.mgr.current()->id == GameId::COLOR_POP);
}

void test_home_and_back_take_priority() {
  Fixture f;
  f.say(Intent::COLORS);
  f.tap(kHomeButton);
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU);
  EXPECT(f.colors.exited == 1 && f.colors.handled == 0);
  f.say(Intent::COLORS);
  f.say(Intent::PAUSE);
  EXPECT(f.mgr.mode() == SceneManager::Mode::PAUSED);
  f.say(Intent::BACK);  // BACK works even while paused
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU && f.colors.exited == 2);
}

void test_pause_resume_and_game_isolation() {
  Fixture f;
  f.say(Intent::COLORS);
  f.say(Intent::RED);
  EXPECT(f.colors.handled == 1 && f.colors.last.intent == Intent::RED);
  f.say(Intent::PAUSE);
  EXPECT(f.colors.paused == 1 && f.colors.handled == 1);
  f.mgr.update(500);
  EXPECT(f.colors.elapsed == 0);  // no updates while paused
  f.say(Intent::RED);             // ignored while paused
  EXPECT(f.colors.handled == 1);
  f.say(Intent::RESUME);
  EXPECT(f.mgr.mode() == SceneManager::Mode::PLAYING && f.colors.resumed == 1);
  f.say(Intent::RESUME);  // stray RESUME never reaches the game
  EXPECT(f.colors.handled == 1);
  f.say(Intent::PAUSE);
  f.tap(SceneManager::kTileRects[0]);  // any tap resumes, and is consumed
  EXPECT(f.mgr.mode() == SceneManager::Mode::PLAYING && f.colors.resumed == 2);
  EXPECT(f.colors.handled == 1);
  f.mgr.update(16);
  EXPECT(f.colors.elapsed == 16);
}

void test_voice_context_filter() {
  Fixture f;
  f.say(Intent::RED);  // not a menu intent
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU);
  f.say(Intent::COLORS);
  f.say(Intent::GO);  // Space intent in Color Pop: dropped
  f.say(Intent::EGGS);  // menu intent during play: dropped
  EXPECT(f.colors.handled == 0 && f.mgr.current()->id == GameId::COLOR_POP);
  f.say(Intent::AGAIN);  // Global intents reach the game
  EXPECT(f.colors.handled == 1);
  EXPECT((f.mgr.active_contexts() & bit(Context::Colors)) != 0);
  EXPECT((f.mgr.active_contexts() & bit(Context::Global)) != 0);
}

void test_touch_debounce_and_mic_button() {
  Fixture f;
  f.say(Intent::COLORS);
  f.mgr.handle(make_touch_event(160, 120, 5000));
  f.mgr.handle(make_touch_event(160, 120, 5050));  // bounce
  EXPECT(f.colors.handled == 1);
  f.mgr.handle(make_touch_event(160, 120, 5080));
  EXPECT(f.colors.handled == 2);
  f.mgr.handle(make_touch_event(kMicButton.x + 10, kMicButton.y + 10, 6000));
  EXPECT(f.colors.handled == 2 && f.mgr.mode() == SceneManager::Mode::PLAYING);
  // Timestamps wrapping past 2^32 still debounce correctly.
  f.mgr.handle(make_touch_event(160, 120, 0xFFFFFFF0u));
  f.mgr.handle(make_touch_event(160, 120, 0x00000010u));
  EXPECT(f.colors.handled == 3);
}

void test_sleep_and_wake() {
  Fixture f;
  f.say(Intent::COLORS);
  f.mgr.update(9u * 60u * 1000u);
  EXPECT(f.mgr.mode() == SceneManager::Mode::PLAYING);
  f.mgr.update(60u * 1000u);
  EXPECT(f.mgr.mode() == SceneManager::Mode::SLEEPING && f.colors.paused == 1);
  EXPECT(f.mgr.active_contexts() == 0);
  f.say(Intent::RED);  // voice cannot wake or act while asleep
  EXPECT(f.mgr.mode() == SceneManager::Mode::SLEEPING);
  const uint32_t before = f.colors.elapsed;
  f.mgr.update(1000);
  EXPECT(f.colors.elapsed == before);
  f.tap(SceneManager::kTileRects[0]);
  EXPECT(f.mgr.mode() == SceneManager::Mode::PLAYING && f.colors.resumed == 1);
  EXPECT(f.colors.handled == 0);  // waking tap consumed
}

void test_activity_resets_idle() {
  Fixture f;
  f.mgr.update(9u * 60u * 1000u);
  f.say(Intent::NEXT);
  f.mgr.update(9u * 60u * 1000u);
  EXPECT(f.mgr.mode() == SceneManager::Mode::MENU);
}

void test_deterministic_seed_per_launch() {
  Fixture a;
  Fixture b;
  a.say(Intent::COLORS);
  b.say(Intent::COLORS);
  EXPECT(a.colors.first_random == b.colors.first_random);
  const uint32_t first = a.colors.first_random;
  a.say(Intent::BACK);
  a.say(Intent::COLORS);
  EXPECT(a.colors.first_random != first);  // replay gets a new, reproducible layout
}

void test_empty_registry_is_safe() {
  RecordingFeedback fb;
  SceneManager mgr(nullptr, 0, fb);
  mgr.handle(make_voice_event(Intent::START, 10));
  mgr.handle(make_touch_event(SceneManager::kNextPageButton.x + 5,
                              SceneManager::kNextPageButton.y + 5, 200));
  mgr.update(16);
  EXPECT(mgr.mode() == SceneManager::Mode::MENU && mgr.page_count() == 0);
}

}  // namespace

int main() {
  test_layout_rules();
  test_paging_and_tile_launch();
  test_voice_launch_and_start();
  test_home_and_back_take_priority();
  test_pause_resume_and_game_isolation();
  test_voice_context_filter();
  test_touch_debounce_and_mic_button();
  test_sleep_and_wake();
  test_activity_resets_idle();
  test_deterministic_seed_per_launch();
  test_empty_registry_is_safe();
  return little_arcade::testing::finish("scene manager tests");
}
