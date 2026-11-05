#include "little_arcade/settings/settings_store.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "little_arcade/settings/parent_gate.hpp"

namespace little_arcade::settings {
namespace {
constexpr const char* kTag = "settings";
constexpr const char* kNamespace = "la_settings";
constexpr const char* kVolumeKey = "volume";
constexpr const char* kMicKey = "mic";
bool g_ready = false;
}  // namespace

bool init_store() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(kTag, "NVS needs erase (%s); settings reset to muted", esp_err_to_name(err));
    if (nvs_flash_erase() == ESP_OK) err = nvs_flash_init();
  }
  g_ready = err == ESP_OK;
  if (!g_ready) ESP_LOGE(kTag, "NVS init failed (%s); sound stays muted", esp_err_to_name(err));
  return g_ready;
}

uint8_t load_volume_level() {
  if (!g_ready) return 0;
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return 0;  // never configured
  uint8_t level = 0;
  if (nvs_get_u8(h, kVolumeKey, &level) != ESP_OK) level = 0;
  nvs_close(h);
  return level > ParentGate::kMaxLevel ? ParentGate::kMaxLevel : level;
}

bool save_volume_level(uint8_t level) {
  if (!g_ready) return false;
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t err = nvs_set_u8(h, kVolumeKey, level);
  if (err == ESP_OK) err = nvs_commit(h);
  nvs_close(h);
  ESP_LOGI(kTag, "volume level %u saved: %s", static_cast<unsigned>(level), esp_err_to_name(err));
  return err == ESP_OK;
}

bool load_mic_enabled() {
  if (!g_ready) return true;
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return true;
  uint8_t on = 1;
  if (nvs_get_u8(h, kMicKey, &on) != ESP_OK) on = 1;
  nvs_close(h);
  return on != 0;
}

bool save_mic_enabled(bool enabled) {
  if (!g_ready) return false;
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t err = nvs_set_u8(h, kMicKey, enabled ? 1 : 0);
  if (err == ESP_OK) err = nvs_commit(h);
  nvs_close(h);
  ESP_LOGI(kTag, "sound jar mic %s saved: %s", enabled ? "on" : "off", esp_err_to_name(err));
  return err == ESP_OK;
}

}  // namespace little_arcade::settings
