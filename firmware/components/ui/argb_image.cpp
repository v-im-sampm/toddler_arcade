#include "little_arcade/ui/argb_image.hpp"

#include <cstdlib>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace little_arcade::ui {

uint32_t* alloc_pixels(std::size_t count) {
  const std::size_t bytes = count * sizeof(uint32_t);
#ifdef ESP_PLATFORM
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
  void* p = std::malloc(bytes);
#endif
  return static_cast<uint32_t*>(p);
}

bool ArgbImage::allocate(int w, int h) {
  if (!px_) {
    px_ = alloc_pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    if (!px_) return false;
    w_ = w;
    h_ = h;
  }
  canvas().clear();
  dsc_ = lv_image_dsc_t{};
  dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc_.header.cf = LV_COLOR_FORMAT_ARGB8888;
  dsc_.header.w = static_cast<uint32_t>(w_);
  dsc_.header.h = static_cast<uint32_t>(h_);
  dsc_.header.stride = static_cast<uint32_t>(w_ * 4);
  dsc_.data_size = static_cast<uint32_t>(w_ * h_ * 4);
  dsc_.data = reinterpret_cast<const uint8_t*>(px_);
  return true;
}

}  // namespace little_arcade::ui
