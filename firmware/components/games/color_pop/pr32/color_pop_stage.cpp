#include "little_arcade/games/color_pop_stage.hpp"

#include <cstdlib>
#include <new>

#include "Arduino.h"
#include "audio/AudioConfig.h"
#include "color_pop_audio.hpp"
#include "color_pop_scene.hpp"
#include "core/Engine.h"
#include "frame_surface.hpp"
#include "graphics/DisplayConfig.h"
#include "input/InputConfig.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#else
#include <chrono>
#endif

namespace little_arcade::color_pop {

namespace {
// Everything the engine needs, placed once (PSRAM on the device) and never
// destroyed: leaving Color Pop only frees the frame buffers.
struct Runtime {
  explicit Runtime(pr32::FrameSurface* s)
      : surface(s),
        engine(pixelroot32::graphics::DisplayConfig::createCustom(s, pr32::FrameSurface::W, pr32::FrameSurface::H),
               pixelroot32::input::InputConfig(),
               pixelroot32::audio::AudioConfig(nullptr, static_cast<int>(pr32::ColorPopAudio::kRateHz))),
        audio(engine.getAudioEngine()) {}

  pr32::FrameSurface* surface;  // owned by the engine's renderer
  pixelroot32::core::Engine engine;
  pr32::ColorPopScene scene;
  pr32::ColorPopAudio audio;
};

Runtime* g_runtime = nullptr;
ColorPopGame* g_game = nullptr;

// Stands in for the APU before init_runtime() (or when it ran out of memory).
class SilentAudio final : public audio::ISceneAudio {
 public:
  bool cue(Cue, uint8_t) override { return false; }
  void stop() override {}
  void render_add(int32_t*, std::size_t) override {}
};

void* runtime_alloc(std::size_t bytes) {
#ifdef ESP_PLATFORM
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
  return std::malloc(bytes);
#endif
}

uint64_t now_us() {
#ifdef ESP_PLATFORM
  return static_cast<uint64_t>(esp_timer_get_time());
#else
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
}  // namespace

bool ColorPopStage::init_runtime() {
  if (g_runtime) return true;
  auto* surface = new (std::nothrow) pr32::FrameSurface();
  if (!surface) return false;
  void* mem = runtime_alloc(sizeof(Runtime));
  if (!mem) {
    delete surface;
    return false;
  }
  g_runtime = new (mem) Runtime(surface);
  g_runtime->engine.init();
  g_runtime->engine.setScene(&g_runtime->scene);
  return true;
}

ColorPopStage& ColorPopStage::instance() {
  static ColorPopStage stage;
  return stage;
}

audio::ISceneAudio& ColorPopStage::scene_audio() {
  static SilentAudio silent;
  if (!g_runtime) return silent;
  return g_runtime->audio;
}

bool ColorPopStage::begin(ColorPopGame& game) {
  if (!g_runtime || !g_runtime->surface->allocate()) return false;
  g_game = &game;
  g_runtime->scene.attach(&game, g_runtime->surface, this);
  reset_stats();
  return true;
}

void ColorPopStage::end() {
  if (!g_runtime) return;
  // Drop any half-finished gesture or queued tap so nothing replays on re-entry.
  g_runtime->engine.getTouchDispatcher().reset();
  g_runtime->scene.attach(nullptr, nullptr, nullptr);
  g_runtime->surface->release();
  g_game = nullptr;
}

bool ColorPopStage::ready() const { return g_runtime && g_runtime->surface->ready() && g_game; }

void ColorPopStage::touch(bool pressed, int16_t x, int16_t y, uint32_t now_ms) {
  if (!g_runtime) return;
  g_runtime->engine.getTouchDispatcher().processTouch(0, pressed, x, y, now_ms);
}

void ColorPopStage::deliver_touch(int16_t x, int16_t y) {
  if (touch_fn_) touch_fn_(touch_user_, x, y, now_ms_);
}

void ColorPopStage::frame(uint32_t now_ms) {
  if (!ready()) return;
  now_ms_ = now_ms;
  little_arcade::pr32_port::set_now_ms(now_ms);
  const uint64_t t0 = now_us();
  g_runtime->engine.run();  // touch events -> scene, draw, present
  if (g_game) g_game->frame_drawn();
  const uint32_t us = static_cast<uint32_t>(now_us() - t0);
  ++frames_;
  frame_us_sum_ += us;
  if (us > frame_us_max_) frame_us_max_ = us;
  dirty_px_sum_ += g_runtime->surface->dirty_pixels();
}

const uint16_t* ColorPopStage::framebuffer() const { return g_runtime ? g_runtime->surface->shown() : nullptr; }
const DirtyRect* ColorPopStage::dirty() const { return g_runtime ? g_runtime->surface->dirty() : nullptr; }
int ColorPopStage::dirty_count() const { return g_runtime ? g_runtime->surface->dirty_count() : 0; }
void ColorPopStage::mark_all_dirty() {
  if (g_runtime) g_runtime->surface->mark_all_dirty();
}

}  // namespace little_arcade::color_pop
