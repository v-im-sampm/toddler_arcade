#pragma once
#include <cstdint>

#include "little_arcade/audio/scene_audio.hpp"
#include "little_arcade/games/color_pop.hpp"

namespace little_arcade::color_pop {

struct DirtyRect {
  int16_t x, y, w, h;
};

// Color Pop's runtime on the PixelRoot32 engine (components/pixelroot32).
// While Color Pop is open the engine owns the frame: Engine::run() delivers
// touch events to the Color Pop scene, which draws through the engine's
// Renderer (baked 4bpp sprites + primitives) into an RGB565 surface; present()
// publishes the changed rows. Sound comes from the engine's APU (see
// scene_audio()). Game logic stays in ColorPopGame, stepped by the arcade's
// SceneManager, so Home, sleep and the parent gate behave as in every game.
//
// The engine is created once (init_runtime) and never destroyed; the frame
// buffers (3 x 150 KiB, PSRAM) exist only between begin() and end().
// UI task only, except scene_audio() whose methods run on the audio task.
class ColorPopStage {
 public:
  using TouchFn = void (*)(void* user, int16_t x, int16_t y, uint32_t now_ms);
  static constexpr int kWidth = 320, kHeight = 240;  // RGB565, native byte order

  // Creates the engine (idempotent). Call on the UI task before the audio
  // task starts. Returns false if memory ran out.
  static bool init_runtime();
  static ColorPopStage& instance();

  // Allocates the frame and paints the static background. False on OOM (or
  // without a runtime): the game still runs, the caller shows a fallback.
  bool begin(ColorPopGame& game);
  void end();
  bool ready() const;

  // Engine TouchDown events are reported here (the arcade forwards them to
  // the SceneManager like any other press).
  void set_touch_handler(TouchFn fn, void* user) {
    touch_fn_ = fn;
    touch_user_ = user;
  }
  // Raw touch state for the engine's touch state machine.
  void touch(bool pressed, int16_t x, int16_t y, uint32_t now_ms);
  // One engine frame: touch events, draw, present. Then game.frame_drawn().
  void frame(uint32_t now_ms);

  // The frame on show (RGB565, native byte order) and what changed in the
  // last frame().
  const uint16_t* framebuffer() const;
  const DirtyRect* dirty() const;
  int dirty_count() const;
  void mark_all_dirty();

  // The engine's APU, wired as AudioFeedback's Color Pop scene audio. Valid
  // after init_runtime() succeeded.
  static audio::ISceneAudio& scene_audio();

  // Diagnostics (reset by reset_stats()).
  uint32_t frames() const { return frames_; }
  uint32_t frame_us_avg() const { return frames_ ? static_cast<uint32_t>(frame_us_sum_ / frames_) : 0; }
  uint32_t frame_us_max() const { return frame_us_max_; }
  uint32_t dirty_px_avg() const { return frames_ ? static_cast<uint32_t>(dirty_px_sum_ / frames_) : 0; }
  void reset_stats() {
    frames_ = frame_us_max_ = 0;
    frame_us_sum_ = dirty_px_sum_ = 0;
  }

  // Called by the scene for each engine TouchDown.
  void deliver_touch(int16_t x, int16_t y);

 private:
  ColorPopStage() = default;

  TouchFn touch_fn_ = nullptr;
  void* touch_user_ = nullptr;
  uint32_t now_ms_ = 0;
  uint32_t frames_ = 0, frame_us_max_ = 0;
  uint64_t frame_us_sum_ = 0, dirty_px_sum_ = 0;
};

}  // namespace little_arcade::color_pop
