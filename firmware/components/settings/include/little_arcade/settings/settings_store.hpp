#pragma once
#include <cstdint>

namespace little_arcade::settings {

// Persists parent settings in the NVS partition (ESP-IDF only). Only the
// parent-chosen volume level is stored: no identifiers and no child data.
// load_volume_level() returns 0 (muted) when nothing has been saved yet or
// NVS is unavailable, so a fresh board always starts silent.
bool init_store();
uint8_t load_volume_level();
bool save_volume_level(uint8_t level);

}  // namespace little_arcade::settings
