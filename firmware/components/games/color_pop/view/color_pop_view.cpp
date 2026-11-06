#include "little_arcade/games/color_pop_view.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>

#include "little_arcade/games/color_pop_paint.hpp"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace little_arcade::color_pop {

namespace c2d = ui::c2d;

namespace {
constexpr int W = ColorPopGame::W, H = ColorPopGame::H;

void* frame_alloc(std::size_t bytes) {
#ifdef ESP_PLATFORM
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return std::malloc(bytes);
#endif
}

void frame_free(void* p) {
#ifdef ESP_PLATFORM
  heap_caps_free(p);
#else
  std::free(p);
#endif
}
}  // namespace

bool ColorPopView::allocate() {
  if (!list_.allocate(display_list_limits())) return false;
  if (!background_) background_ = static_cast<uint32_t*>(frame_alloc(sizeof(uint32_t) * W * H));
  if (!front_) front_ = static_cast<uint16_t*>(frame_alloc(sizeof(uint16_t) * W * H));
  if (!background_ || !front_) {
    release();
    return false;
  }
  // paintBg() once; every frame composites over this layer.
  ctx_.reset();
  paint_background(ctx_);
  c2d::Target t;
  t.out32 = background_;
  c2d::Renderer::render(list_, t, nullptr);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) front_[y * W + x] = c2d::to565(background_[y * W + x], x, y);
  }
  return true;
}

void ColorPopView::release() {
  frame_free(background_);
  frame_free(front_);
  background_ = nullptr;
  front_ = nullptr;
  image_ = nullptr;
  cache_.invalidate();
}

void ColorPopView::create(lv_obj_t* parent) {
  image_ = nullptr;
  painted_ = false;
  cache_.invalidate();
  if (!allocate()) {
    // Out of memory: the game logic still runs; show the sea colour only.
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x9ED8F0), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    return;
  }
  dsc_ = lv_image_dsc_t{};
  dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
  dsc_.header.w = W;
  dsc_.header.h = H;
  dsc_.header.stride = W * 2;
  dsc_.data_size = sizeof(uint16_t) * W * H;
  dsc_.data = reinterpret_cast<const uint8_t*>(front_);
  image_ = lv_image_create(parent);
  lv_obj_remove_flag(image_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(image_, 0, 0);
  lv_image_set_src(image_, &dsc_);
}

void ColorPopView::sync(ColorPopGame& game, uint32_t now_ms) {
  (void)now_ms;
  if (!image_ || !front_) return;
  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();
  ctx_.reset();
  paint_frame(ctx_, game, game.voice_enabled());
  const auto t1 = clock::now();
  c2d::Target t;
  t.background = background_;
  t.out565 = front_;
  const c2d::RenderStats stats = c2d::Renderer::render(list_, t, &cache_, c2d::second_core_worker());
  const auto t2 = clock::now();
  const uint32_t paint_us = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
  const uint32_t render_us = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
  paint_us_sum_ += paint_us;
  render_us_sum_ += render_us;
  paint_us_max_ = paint_us > paint_us_max_ ? paint_us : paint_us_max_;
  render_us_max_ = render_us > render_us_max_ ? render_us : render_us_max_;
  ++syncs_;
  game.frame_drawn();
  if (!painted_) {
    painted_ = true;
    lv_obj_invalidate(image_);
  } else {
    for (int i = 0; i < stats.rect_count; ++i) {
      const int16_t* r = stats.rects[i];
      const lv_area_t a{r[0], r[1], static_cast<int32_t>(r[0] + r[2] - 1), static_cast<int32_t>(r[1] + r[3] - 1)};
      lv_obj_invalidate_area(image_, &a);
    }
  }
  if (stats.tiles_drawn > 0) {
    ++frames_;
    tiles_drawn_ += static_cast<uint32_t>(stats.tiles_drawn);
    tiles_worker_ += static_cast<uint32_t>(stats.tiles_by_worker);
  }
}

}  // namespace little_arcade::color_pop
