#pragma once
#include <cstdint>

#include "graphics/BaseDrawSurface.h"
#include "little_arcade/games/color_pop_stage.hpp"

namespace little_arcade::color_pop::pr32 {

// The PixelRoot32 DrawSurface Color Pop renders into: a 320x240 RGB565
// (native byte order) work frame plus a cached static background and the
// frame on show. Every write records the touched x-span per 16-row band, so a
// frame restores only what the previous frame drew and present() diffs only
// those spans against the shown frame. Buffers live in PSRAM on the device
// and are held only while Color Pop is open.
class FrameSurface final : public pixelroot32::graphics::BaseDrawSurface {
 public:
  static constexpr int W = 320, H = 240;
  static constexpr int kBand = 16, kBands = H / kBand;

  bool allocate();
  void release();
  bool ready() const { return work_ != nullptr; }

  // Copies the work frame (holding the static background) to the cache and
  // forgets the spans drawn so far.
  void save_background();
  // Erases what the previous frame drew (restores those spans from the cache).
  void begin_frame();
  void mark_all_dirty() { full_ = true; }

  const uint16_t* shown() const { return shown_; }
  const DirtyRect* dirty() const { return dirty_; }
  int dirty_count() const { return dirty_n_; }
  uint32_t dirty_pixels() const { return dirty_px_; }

  // DrawSurface
  void init() override {}
  void clearBuffer() override {}
  void sendBuffer() override;
  void drawPixel(int x, int y, uint16_t color) override;
  void drawFilledRectangle(int x, int y, int w, int h, uint16_t color) override;
  void drawFilledCircle(int x0, int y0, int r, uint16_t color) override;
  uint16_t* getPixelBuffer() override { return work_; }

 private:
  struct Span {
    int16_t lo, hi;  // [lo, hi), empty when lo >= hi
  };
  void touch(int band, int lo, int hi) {
    Span& s = drawn_[band];
    if (lo < s.lo) s.lo = static_cast<int16_t>(lo);
    if (hi > s.hi) s.hi = static_cast<int16_t>(hi);
  }
  void hspan(int y, int x0, int x1, uint16_t color);  // inclusive, clipped
  void reset_spans(Span* spans);

  uint16_t* work_ = nullptr;
  uint16_t* shown_ = nullptr;
  uint16_t* bg_ = nullptr;
  Span drawn_[kBands]{};
  Span prev_[kBands]{};
  DirtyRect dirty_[kBands]{};
  int dirty_n_ = 0;
  uint32_t dirty_px_ = 0;
  bool full_ = true;
};

}  // namespace little_arcade::color_pop::pr32
