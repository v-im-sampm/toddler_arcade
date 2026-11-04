// Little Arcade P01 build skeleton.
// Boots, reports build/board/memory facts and idles. It deliberately does not
// touch any GPIO: display/touch bring-up is H02 and audio is H03.
#include <cinttypes>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/intent.hpp"
#include "lvgl.h"

namespace {
constexpr const char* kTag = "little_arcade";
constexpr uint32_t kHeapReportPeriodMs = 10000;

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
    ESP_LOGW(kTag, "Pins are not bench-verified; peripheral drivers are not started.");
  }
}
}  // namespace

extern "C" void app_main(void) {
  log_memory("boot");
  log_board();
  lv_init();
  log_memory("lv_init");

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kHeapReportPeriodMs));
    log_memory("idle");
  }
}
