#include <cinttypes>
#include <cstdio>

#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game/aquarium.hpp"
#include "ui/aquarium_view.hpp"
#include "little_arcade/board_io.hpp"
#include "little_arcade/settings/settings_store.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/i2s_audio.hpp"
#include "little_arcade/board_pins.hpp"
#include "lvgl.h"
#include "sprite_pack.hpp"

namespace {
constexpr const char* tag = "aquarium_sprite";
constexpr int screen_w = 320, screen_h = 240, buffer_lines = 24;
constexpr size_t buffer_bytes = screen_w * buffer_lines * 2;
esp_lcd_panel_handle_t panel = nullptr;
uint64_t frame_pixels = 0;
uint32_t clock_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void flush_done(void* ctx) { lv_display_flush_ready(static_cast<lv_display_t*>(ctx)); }
void flush(lv_display_t*, const lv_area_t* area, uint8_t* pixels) {
  const uint32_t count = static_cast<uint32_t>(lv_area_get_width(area) * lv_area_get_height(area));
  lv_draw_sw_rgb565_swap(pixels, count);
  frame_pixels += count;
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
}
void memory_log(const char* label) {
  ESP_LOGI(tag, "[mem:%s] internal=%u min=%u psram=%u min=%u",
           label, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}

class DeviceAudio final : public aquarium::Audio {
public:
  DeviceAudio() : sound_(speaker_config()) {}
  static little_arcade::VolumeLimiter::Config speaker_config() {
    little_arcade::VolumeLimiter::Config cfg;
    cfg.sample_rate_hz = little_arcade::audio::kSampleRateHz;
    cfg.peak_limit = little_arcade::VolumeLimiter::amp_safe_peak(
        little_arcade::board::audio::kAmpGainDb, little_arcade::board::audio::kAmpSupplyMv);
    return cfg;
  }
  void play(aquarium::Cue cue, int variant) override {
    using little_arcade::Cue;
    if (!playing_) return;
    switch (cue) {
      case aquarium::Cue::Welcome: sound_.play(Cue::PROMPT, 0); break;
      case aquarium::Cue::Feed: sound_.play(Cue::TAP, static_cast<uint8_t>(variant)); break;
      case aquarium::Cue::Plip: sound_.play(Cue::TAP, 0); break;
      case aquarium::Cue::Gulp: sound_.play(Cue::SUCCESS, static_cast<uint8_t>(variant)); break;
      case aquarium::Cue::Happy: sound_.play(Cue::REACTION, static_cast<uint8_t>(variant)); break;
      case aquarium::Cue::Bubble: sound_.play(Cue::NOTE, 4); break;
      case aquarium::Cue::Pop: sound_.play(Cue::NOTE, 3); break;
      case aquarium::Cue::Glass: sound_.play(Cue::NOTE, 2); break;
      case aquarium::Cue::Fish: sound_.play(Cue::REACTION, static_cast<uint8_t>(variant)); break;
      case aquarium::Cue::Visitor: sound_.play(Cue::NOTE, static_cast<uint8_t>(7 + variant)); break;
      case aquarium::Cue::Bed: sound_.play(Cue::NOTE, static_cast<uint8_t>(5 + variant)); break;
      case aquarium::Cue::Hint: sound_.play(Cue::PROMPT, 0); break;
    }
  }
  void bedtime(bool) override {}
  void playing(bool on) override { playing_ = on; }
  bool start(uint8_t level) {
    sound_.set_level(level);
    little_arcade::audio::I2sAudioOptions options;
    options.mic_level_probe = false;
    return little_arcade::audio::start_i2s_audio(sound_, options);
  }
private:
  little_arcade::audio::AudioFeedback sound_;
  bool playing_ = false;
};

void run(void*) {
  uint32_t flash_size = 0;
  ESP_ERROR_CHECK(esp_flash_get_size(nullptr, &flash_size));
  ESP_LOGI(tag, "flash=%" PRIu32 " psram=%u asset pack=%u bytes",
           flash_size, static_cast<unsigned>(esp_psram_is_initialized() ? esp_psram_get_size() : 0),
           static_cast<unsigned>(aquarium::pack::bytes));
  if (flash_size < 16 * 1024 * 1024 || !esp_psram_is_initialized()) {
    ESP_LOGE(tag, "N16R8 flash and PSRAM required; refusing to display sprites");
    vTaskDelete(nullptr);
    return;
  }
  lv_init();
  lv_tick_set_cb(clock_ms);
  auto* display = lv_display_create(screen_w, screen_h);
  little_arcade::board::DisplayConfig cfg;
  cfg.pclk_hz = 40 * 1000 * 1000;
  cfg.max_transfer_bytes = buffer_bytes;
  cfg.on_flush_done = flush_done;
  cfg.ctx = display;
  ESP_ERROR_CHECK(little_arcade::board::display_init(cfg, &panel));
  void* first = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* second = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  configASSERT(first && second);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, first, second, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  const esp_err_t touch = little_arcade::board::touch_init();
  if (touch != ESP_OK) ESP_LOGE(tag, "touch unavailable: %s", esp_err_to_name(touch));
  memory_log("before_assets");
  static DeviceAudio audio;
  static aquarium::Game game(audio, 1234);
  static aquarium::View view;
  view.create(lv_screen_active(), "");
  memory_log("after_assets");
  view.sync(game);
  lv_refr_now(display);
  little_arcade::board::backlight_set(true);
  little_arcade::settings::init_store();
  const uint8_t volume = little_arcade::settings::load_volume_level();
  if (!audio.start(volume)) ESP_LOGE(tag, "audio unavailable; touchscreen game remains playable");
  ESP_LOGI(tag, "saved parent volume=%u (0 = muted); ready", volume);

  bool pressed = false;
  uint32_t last = clock_ms(), report = last;
  uint32_t frames = 0;
  for (;;) {
    const uint32_t now = clock_ms();
    if (touch == ESP_OK) {
      little_arcade::board::TouchSample sample{};
      if (little_arcade::board::touch_read(&sample) == ESP_OK && sample.pressed) {
        const auto point = little_arcade::board::ns2009_to_screen(sample.raw_x, sample.raw_y);
        if (!pressed) game.down(point.x, point.y);
        else game.move(point.x, point.y);
        pressed = true;
      } else if (pressed) {
        game.up();
        pressed = false;
      }
    }
    game.tick(static_cast<float>(now - last) / 1000.0f);
    last = now;
    view.sync(game);
    const uint64_t before = frame_pixels;
    lv_timer_handler();
    if (frame_pixels > before) ++frames;
    if (now - report >= 10000) {
      ESP_LOGI(tag, "[device] frames/10s=%u meals=%d visits=%d bubbles=%d pellets=%d ui_stack_free=%u",
               frames, game.meals(), game.visits(), game.bubble_count(), game.pellet_count(),
               static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
      memory_log("playing");
      frames = 0; report = now;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}
extern "C" void app_main() {
  BaseType_t created = xTaskCreatePinnedToCore(run, "aquarium_ui", 24576, nullptr, 4, nullptr, 1);
  configASSERT(created == pdPASS);
}
