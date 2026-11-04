// Little Arcade firmware: H02 display/touch bring-up running Color Pop.
// Single UI task owns LVGL (docs/architecture.md). No audio yet (H03 blocked):
// cues are logged. Touch events are logged raw + mapped so orientation and
// calibration can be checked on the bench.
#include <cinttypes>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "little_arcade/board_io.hpp"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_view.hpp"
#include "little_arcade/intent.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "lvgl.h"

namespace {
using namespace little_arcade;
using namespace little_arcade::color_pop;

constexpr const char* kTag = "little_arcade";
constexpr uint32_t kStatsPeriodMs = 10000;
constexpr int kBufLines = 24;  // two partial RGB565 buffers, 15 KB each
constexpr std::size_t kBufBytes = kScreenWidth * kBufLines * 2;

void log_memory(const char* phase) {
  ESP_LOGI(kTag,
           "[mem:%s] internal free=%u min=%u largest=%u | psram free=%u min=%u", phase,
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}

void log_board() {
  namespace b = little_arcade::board;
  esp_chip_info_t chip{};
  esp_chip_info(&chip);
  uint32_t flash_bytes = 0;
  esp_flash_get_size(nullptr, &flash_bytes);
  const esp_app_desc_t* app = esp_app_get_description();

  ESP_LOGI(kTag, "app=%s idf=%s cores=%d rev=%d.%d", app->version, app->idf_ver, chip.cores,
           chip.revision / 100, chip.revision % 100);
  ESP_LOGI(kTag, "flash=%" PRIu32 " bytes, psram=%u bytes", flash_bytes,
           static_cast<unsigned>(esp_psram_is_initialized() ? esp_psram_get_size() : 0));
  ESP_LOGI(kTag, "lvgl=%d.%d.%d intents=%u", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           LVGL_VERSION_PATCH, static_cast<unsigned>(little_arcade::kIntentCount));
  ESP_LOGW(kTag, "board pins: verified=%s level=%s schematic=%s pcb=%s",
           b::kPinsVerified ? "true" : "false", b::kVerificationLevel, b::kSchematicRevision,
           b::kPhysicalPcbRevision);
  if (!b::kPinsVerified) {
    ESP_LOGW(kTag, "H02 bring-up: starting display/touch on source-reviewed pins.");
  }
}

class LogFeedback final : public IFeedback {
 public:
  void play(Cue cue, uint8_t variant) override {
    ESP_LOGI("audio", "cue=%u variant=%u (no audio until H03)", static_cast<unsigned>(cue),
             static_cast<unsigned>(variant));
  }
};

struct Scene {
  Rng rng{1};
  LogFeedback feedback;
  GameContext ctx{rng, feedback};
  ColorPopGame game;
  ColorPopView view;
  ui::ChromeView chrome;

  void restart() {
    if (game.active()) game.exit();
    rng.reseed(esp_random() | 1u);
    game.enter(ctx);
  }
};

Scene g_scene;
esp_lcd_panel_handle_t g_panel = nullptr;
uint64_t g_frame_pixels = 0;
bool g_was_pressed = false;

uint32_t tick_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void flush_done(void* ctx) { lv_display_flush_ready(static_cast<lv_display_t*>(ctx)); }

void flush(lv_display_t*, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(px_map, static_cast<uint32_t>(w * h));  // ST7789 expects big-endian RGB565
  g_frame_pixels += static_cast<uint64_t>(w * h);
  esp_lcd_panel_draw_bitmap(g_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

void read_touch(lv_indev_t*, lv_indev_data_t* data) {
  board::TouchSample s{};
  const esp_err_t err = board::touch_read(&s);
  if (err != ESP_OK || !s.pressed) {
    data->state = LV_INDEV_STATE_RELEASED;
    g_was_pressed = false;
    return;
  }
  const board::ScreenPoint p = board::ns2009_to_screen(s.raw_x, s.raw_y);
  data->point.x = p.x;
  data->point.y = p.y;
  data->state = LV_INDEV_STATE_PRESSED;
  if (!g_was_pressed) {
    ESP_LOGI("touch", "down raw=(%u,%u) z=%u -> screen=(%d,%d)", s.raw_x, s.raw_y, s.raw_z, p.x, p.y);
  }
  g_was_pressed = true;
}

void on_press(lv_event_t*) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  const auto x = static_cast<int16_t>(p.x), y = static_cast<int16_t>(p.y);
  if (kHomeButton.contains(x, y)) {
    ESP_LOGI(kTag, "[nav] Home -> restart Color Pop (launcher not built yet)");
    g_scene.restart();
  } else if (kMicButton.contains(x, y)) {
    ESP_LOGI(kTag, "[nav] Mic pressed (voice needs H03/V01)");
  } else {
    g_scene.game.handle(make_touch_event(x, y, tick_ms()));
  }
}

lv_display_t* start_display() {
  lv_display_t* disp = lv_display_create(kScreenWidth, kScreenHeight);
  board::DisplayConfig cfg;
  cfg.max_transfer_bytes = kBufBytes;
  cfg.on_flush_done = flush_done;
  cfg.ctx = disp;
  ESP_ERROR_CHECK(board::display_init(cfg, &g_panel));

  void* b1 = heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* b2 = heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  configASSERT(b1 && b2);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, b1, b2, kBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush);
  return disp;
}

}  // namespace

constexpr uint32_t kUiStackBytes = 16 * 1024;  // LVGL SW render overflowed the 3.5 KB main stack

void ui_task(void*) {
  lv_init();
  lv_tick_set_cb(tick_ms);
  start_display();

  const esp_err_t touch_err = board::touch_init();
  if (touch_err == ESP_OK) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_touch);
  } else {
    ESP_LOGE(kTag, "touch init failed (%s); display only", esp_err_to_name(touch_err));
  }

  lv_obj_t* screen = lv_screen_active();
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  g_scene.view.create(screen);
  g_scene.chrome.create(screen);
  lv_obj_add_event_cb(screen, on_press, LV_EVENT_PRESSED, nullptr);
  g_scene.restart();

  // First full frame before the backlight comes on: no flash of garbage.
  g_scene.view.sync(g_scene.game, tick_ms());
  lv_refr_now(nullptr);
  board::backlight_set(true);
  log_memory("ui_ready");

  uint32_t last = tick_ms(), stats_start = last, frames = 0;
  int64_t render_us_sum = 0, render_us_max = 0;
  uint64_t pixels_sum = 0;
  for (;;) {
    const uint32_t now = tick_ms();
    g_scene.game.update(now - last);
    last = now;
    g_scene.view.sync(g_scene.game, now);
    g_frame_pixels = 0;
    const int64_t t0 = esp_timer_get_time();
    const uint32_t idle_ms = lv_timer_handler();
    const int64_t dt = esp_timer_get_time() - t0;
    if (g_frame_pixels) {
      ++frames;
      render_us_sum += dt;
      render_us_max = dt > render_us_max ? dt : render_us_max;
      pixels_sum += g_frame_pixels;
    }
    if (now - stats_start >= kStatsPeriodMs) {
      lv_mem_monitor_t mem;
      lv_mem_monitor(&mem);
      ESP_LOGI("perf",
               "[device] %u redraws/10s, render avg=%lld us max=%lld us, px avg=%llu, rounds=%u, lv_mem max_used=%u, "
               "ui stack free=%u",
               static_cast<unsigned>(frames), frames ? render_us_sum / frames : 0LL, render_us_max,
               frames ? pixels_sum / frames : 0ULL, static_cast<unsigned>(g_scene.game.rounds_completed()),
               static_cast<unsigned>(mem.max_used),
               static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
      log_memory("ui");
      stats_start = now;
      frames = 0;
      render_us_sum = render_us_max = 0;
      pixels_sum = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(idle_ms < 5 ? 5 : (idle_ms > 20 ? 20 : idle_ms)));
  }
}

extern "C" void app_main(void) {
  log_memory("boot");
  log_board();
  xTaskCreatePinnedToCore(ui_task, "ui", kUiStackBytes, nullptr, 5, nullptr, 1);
}
