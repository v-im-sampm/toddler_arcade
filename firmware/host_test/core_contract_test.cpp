#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "little_arcade/board_pins.hpp"
#include "little_arcade/game.hpp"
#include "little_arcade/input.hpp"
#include "little_arcade/intent.hpp"

using namespace little_arcade;

namespace {
int g_failures = 0;
void expect(bool ok, const char* expr, int line) {
  if (!ok) {
    std::printf("FAIL %s:%d: %s\n", __FILE__, line, expr);
    ++g_failures;
  }
}
#define EXPECT(cond) expect(static_cast<bool>(cond), #cond, __LINE__)

static_assert(static_cast<int>(Intent::UNKNOWN) == 0, "UNKNOWN must be 0");
static_assert(intent_allowed(Intent::RED, static_cast<uint32_t>(Context::Colors)), "constexpr filter");

void test_intent_table() {
  EXPECT(kIntentCount == 28);
  std::set<int> ids;
  std::set<std::string> names;
  int previous = 0;
  for (const auto& info : kIntentTable) {
    const int id = static_cast<int>(info.intent);
    EXPECT(id > previous);
    previous = id;
    ids.insert(id);
    names.insert(info.name);
    EXPECT(info.context_mask != 0);
    EXPECT(std::strcmp(intent_name(info.intent), info.name) == 0);
  }
  EXPECT(ids.size() == kIntentCount);
  EXPECT(names.size() == kIntentCount);
  EXPECT(std::strcmp(intent_name(Intent::UNKNOWN), "UNKNOWN") == 0);
  EXPECT(intent_info(Intent::UNKNOWN) == nullptr);
}

void test_context_filter() {
  const uint32_t color_pop = Context::Colors | Context::Global;
  EXPECT(intent_allowed(Intent::RED, color_pop));
  EXPECT(intent_allowed(Intent::BACK, color_pop));
  EXPECT(intent_allowed(Intent::AGAIN, color_pop));
  EXPECT(!intent_allowed(Intent::GO, color_pop));
  EXPECT(!intent_allowed(Intent::START, color_pop));
  EXPECT(!intent_allowed(Intent::UNKNOWN, color_pop));

  const uint32_t rocket = Context::Space | Context::Global;
  EXPECT(intent_allowed(Intent::GO, rocket));
  EXPECT(intent_allowed(Intent::STOP, rocket));
  EXPECT(!intent_allowed(Intent::BLUE, rocket));
  EXPECT(!intent_allowed(Intent::RED, 0));
}

class MockGame final : public IGame {
 public:
  int entered = 0, exited = 0, reds = 0, rejected = 0;
  uint32_t elapsed = 0;
  void enter(GameContext& context) override {
    ++entered;
    seed = context.rng_seed;
  }
  void update(uint32_t dt_ms) override { elapsed += dt_ms; }
  void handle(const InputEvent& e) override {
    if (!intent_allowed(e.intent, active_contexts()) && e.source == Source::VOICE) {
      ++rejected;
      return;
    }
    if (e.intent == Intent::RED) ++reds;
  }
  void exit() override { ++exited; }
  uint32_t active_contexts() const override { return Context::Colors | Context::Global; }
  uint32_t seed = 0;
};

void test_game_lifecycle_touch_and_voice_equivalent() {
  MockGame game;
  GameContext ctx;
  ctx.rng_seed = 1234;
  IGame& scene = game;
  scene.enter(ctx);
  scene.handle(make_voice_event(Intent::RED, 10));
  scene.handle(make_touch_event(40, 60, 20, Intent::RED));
  scene.handle(make_voice_event(Intent::GO, 30));
  scene.update(16);
  scene.update(17);
  scene.exit();
  EXPECT(game.entered == 1 && game.exited == 1);
  EXPECT(game.seed == 1234);
  EXPECT(game.reds == 2);
  EXPECT(game.rejected == 1);
  EXPECT(game.elapsed == 33);

  const InputEvent v = make_voice_event(Intent::YES, 5);
  EXPECT(v.x == kNoCoordinate && v.y == kNoCoordinate && v.source == Source::VOICE);
}

void test_default_context_is_safe() {
  GameContext ctx;
  EXPECT(!ctx.audio_enabled);
  EXPECT(!ctx.voice_enabled);
}

void test_board_pins_gate() {
  namespace b = little_arcade::board;
  EXPECT(b::audio::kBclk == b::kPinTbd);
  EXPECT(b::audio::kWs == b::kPinTbd);
  EXPECT(b::audio::kMicDataIn == b::kPinTbd);
  EXPECT(b::audio::kAmpDataOut == b::kPinTbd);
  EXPECT(b::audio::kAmpEnable == b::kPinTbd);
  const int assigned[] = {b::display::kSclk, b::display::kMosi, b::display::kMiso, b::display::kCs,
                          b::display::kDc,   b::display::kRst,  b::display::kBacklight,
                          b::touch::kScl,    b::touch::kSda,    b::touch::kPenirq,
                          b::sd::kClk,       b::sd::kCmd,       b::sd::kD0};
  std::set<int> unique;
  for (int pin : assigned) {
    if (pin != b::kPinTbd) EXPECT(unique.insert(pin).second);
  }
}
}  // namespace

int main() {
  test_intent_table();
  test_context_filter();
  test_game_lifecycle_touch_and_voice_equivalent();
  test_default_context_is_safe();
  test_board_pins_gate();
  if (g_failures) {
    std::printf("%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("PASS: host core contract tests (host only; not device validation)\n");
  return 0;
}
