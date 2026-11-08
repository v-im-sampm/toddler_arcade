#pragma once

#include <cstdint>

#include "lvgl.h"

namespace aquarium {

class ArgbImage {
 public:
  bool allocate(int width, int height);
  uint32_t* pixels() { return pixels_; }
  const lv_image_dsc_t* dsc() const { return &descriptor_; }

 private:
  uint32_t* pixels_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  lv_image_dsc_t descriptor_{};
};

}  // namespace aquarium
