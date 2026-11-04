#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/ui/paint.hpp"
#include "lvgl.h"

namespace little_arcade::ui {

// Pixel storage for painted art: PSRAM on the device (internal RAM is kept
// for DMA and stacks), the C heap on the desktop. Called only while views are
// being built; the buffers live for the whole program and are never freed.
uint32_t* alloc_pixels(std::size_t count);

// ARGB8888 image painted with ui::Canvas and shown with an lv_image.
class ArgbImage {
 public:
  // Allocates w x h pixels and clears them. Returns false if memory is short.
  bool allocate(int w, int h);
  bool ready() const { return px_ != nullptr; }
  Canvas canvas() { return Canvas{px_, w_, h_}; }
  const lv_image_dsc_t* dsc() const { return &dsc_; }
  int width() const { return w_; }
  int height() const { return h_; }

 private:
  uint32_t* px_ = nullptr;
  int w_ = 0;
  int h_ = 0;
  lv_image_dsc_t dsc_{};
};

}  // namespace little_arcade::ui
