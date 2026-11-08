#include "ui/argb_image.hpp"

#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace aquarium {

bool ArgbImage::allocate(int width, int height) {
  if (!pixels_) {
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
#ifdef ESP_PLATFORM
    void* memory = heap_caps_malloc(count * sizeof(uint32_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) memory = heap_caps_malloc(count * sizeof(uint32_t), MALLOC_CAP_8BIT);
#else
    void* memory = std::malloc(count * sizeof(uint32_t));
#endif
    if (!memory) return false;
    pixels_ = static_cast<uint32_t*>(memory);
    width_ = width;
    height_ = height;
  }

  const auto bytes = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * sizeof(uint32_t);
  std::memset(pixels_, 0, bytes);
  descriptor_ = lv_image_dsc_t{};
  descriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
  descriptor_.header.cf = LV_COLOR_FORMAT_ARGB8888;
  descriptor_.header.w = static_cast<uint32_t>(width_);
  descriptor_.header.h = static_cast<uint32_t>(height_);
  descriptor_.header.stride = static_cast<uint32_t>(width_ * 4);
  descriptor_.data_size = static_cast<uint32_t>(bytes);
  descriptor_.data = reinterpret_cast<const uint8_t*>(pixels_);
  return true;
}

}  // namespace aquarium
