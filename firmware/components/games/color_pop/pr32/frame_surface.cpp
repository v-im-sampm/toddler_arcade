#include "frame_surface.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace little_arcade::color_pop::pr32 {

namespace {
constexpr std::size_t kFrameBytes = sizeof(uint16_t) * FrameSurface::W * FrameSurface::H;

uint16_t* frame_alloc() {
#ifdef ESP_PLATFORM
  return static_cast<uint16_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  return static_cast<uint16_t*>(std::malloc(kFrameBytes));
#endif
}

void frame_free(uint16_t* p) {
#ifdef ESP_PLATFORM
  heap_caps_free(p);
#else
  std::free(p);
#endif
}
}  // namespace

bool FrameSurface::allocate() {
  if (!work_) work_ = frame_alloc();
  if (!shown_) shown_ = frame_alloc();
  if (!bg_) bg_ = frame_alloc();
  if (!work_ || !shown_ || !bg_) {
    release();
    return false;
  }
  std::memset(work_, 0, kFrameBytes);
  reset_spans(drawn_);
  reset_spans(prev_);
  dirty_n_ = 0;
  full_ = true;
  return true;
}

void FrameSurface::release() {
  frame_free(work_);
  frame_free(shown_);
  frame_free(bg_);
  work_ = shown_ = bg_ = nullptr;
  dirty_n_ = 0;
}

void FrameSurface::reset_spans(Span* spans) {
  for (int b = 0; b < kBands; ++b) spans[b] = Span{W, 0};
}

void FrameSurface::save_background() {
  if (!ready()) return;
  std::memcpy(bg_, work_, kFrameBytes);
  reset_spans(drawn_);
  reset_spans(prev_);
  full_ = true;
}

void FrameSurface::begin_frame() {
  if (!ready()) return;
  for (int b = 0; b < kBands; ++b) {
    const Span s = prev_[b];
    if (s.lo >= s.hi) continue;
    const std::size_t n = sizeof(uint16_t) * static_cast<std::size_t>(s.hi - s.lo);
    for (int y = b * kBand; y < (b + 1) * kBand; ++y) {
      std::memcpy(work_ + y * W + s.lo, bg_ + y * W + s.lo, n);
    }
  }
}

void FrameSurface::sendBuffer() {
  dirty_n_ = 0;
  dirty_px_ = 0;
  if (!ready()) return;
  if (full_) {
    full_ = false;
    std::memcpy(shown_, work_, kFrameBytes);
    dirty_[dirty_n_++] = DirtyRect{0, 0, W, H};
    dirty_px_ = W * H;
  } else {
    for (int b = 0; b < kBands; ++b) {
      // Anything that changed was drawn this frame or erased from last frame.
      const int lo = drawn_[b].lo < prev_[b].lo ? drawn_[b].lo : prev_[b].lo;
      const int hi = drawn_[b].hi > prev_[b].hi ? drawn_[b].hi : prev_[b].hi;
      if (lo >= hi) continue;
      int min_x = W, max_x = -1;
      for (int y = b * kBand; y < (b + 1) * kBand; ++y) {
        const uint16_t* a = work_ + y * W;
        const uint16_t* s = shown_ + y * W;
        int x0 = lo;
        while (x0 < hi && a[x0] == s[x0]) ++x0;
        if (x0 == hi) continue;
        int x1 = hi - 1;
        while (a[x1] == s[x1]) --x1;
        if (x0 < min_x) min_x = x0;
        if (x1 > max_x) max_x = x1;
      }
      if (max_x < 0) continue;
      const int w = max_x - min_x + 1;
      for (int y = b * kBand; y < (b + 1) * kBand; ++y) {
        std::memcpy(shown_ + y * W + min_x, work_ + y * W + min_x, sizeof(uint16_t) * static_cast<std::size_t>(w));
      }
      dirty_[dirty_n_++] = DirtyRect{static_cast<int16_t>(min_x), static_cast<int16_t>(b * kBand),
                                     static_cast<int16_t>(w), static_cast<int16_t>(kBand)};
      dirty_px_ += static_cast<uint32_t>(w * kBand);
    }
  }
  for (int b = 0; b < kBands; ++b) prev_[b] = drawn_[b];
  reset_spans(drawn_);
}

void FrameSurface::drawPixel(int x, int y, uint16_t color) {
  if (static_cast<unsigned>(x) >= static_cast<unsigned>(W) || static_cast<unsigned>(y) >= static_cast<unsigned>(H) ||
      !work_) {
    return;
  }
  work_[y * W + x] = color;
  touch(y / kBand, x, x + 1);
}

void FrameSurface::hspan(int y, int x0, int x1, uint16_t color) {
  if (static_cast<unsigned>(y) >= static_cast<unsigned>(H)) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= W) x1 = W - 1;
  if (x0 > x1) return;
  uint16_t* p = work_ + y * W;
  for (int x = x0; x <= x1; ++x) p[x] = color;
  touch(y / kBand, x0, x1 + 1);
}

void FrameSurface::drawFilledRectangle(int x, int y, int w, int h, uint16_t color) {
  if (!work_ || w <= 0 || h <= 0) return;
  for (int j = 0; j < h; ++j) hspan(y + j, x, x + w - 1, color);
}

void FrameSurface::drawFilledCircle(int x0, int y0, int r, uint16_t color) {
  if (!work_ || r < 0) return;
  const float rr = (static_cast<float>(r) + 0.5f) * (static_cast<float>(r) + 0.5f);
  for (int dy = -r; dy <= r; ++dy) {
    const int hw = static_cast<int>(std::sqrt(rr - static_cast<float>(dy * dy)));
    hspan(y0 + dy, x0 - hw, x0 + hw, color);
  }
}

}  // namespace little_arcade::color_pop::pr32
