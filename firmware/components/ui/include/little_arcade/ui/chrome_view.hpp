#pragma once
#include <cstdint>

#include "little_arcade/ui/a8_raster.hpp"
#include "lvgl.h"

namespace little_arcade::ui {

// Owned A8 image built once with rasterize_a8(). Draw it with an lv_image
// whose image_recolor style supplies the colour.
template <int W, int H>
struct A8Image {
  uint8_t pixels[W * H];
  lv_image_dsc_t dsc;

  template <typename Inside>
  const lv_image_dsc_t* build(Inside inside) {
    rasterize_a8(pixels, W, H, inside);
    dsc = lv_image_dsc_t{};
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_A8;
    dsc.header.w = W;
    dsc.header.h = H;
    dsc.header.stride = W;
    dsc.data_size = sizeof(pixels);
    dsc.data = pixels;
    return &dsc;
  }
};

// Home (top-left) and push-to-talk Mic (top-right) controls. Visual only: the
// SceneManager and speech glue own their behaviour. Hit rects are
// kHomeButton and kMicButton.
class ChromeView {
 public:
  void create(lv_obj_t* parent);
  // The launcher has no Home button; games show it.
  void set_home_visible(bool visible);
  // Hides both keys (games that draw their own chrome, e.g. Color Pop). Hit
  // rects are unchanged: SceneManager still handles them.
  void set_visible(bool visible);

 private:
  static constexpr int kIcon = 40;
  A8Image<kIcon, kIcon> home_{};
  A8Image<kIcon, kIcon> mic_{};
  lv_obj_t* home_btn_ = nullptr;
  lv_obj_t* mic_btn_ = nullptr;
  bool home_visible_ = true;
  bool visible_ = true;
  void apply();
};

lv_color_t to_lv(uint32_t rgb);

}  // namespace little_arcade::ui
