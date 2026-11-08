#pragma once
// Reader for sound_jar.pack (written by tools/export_assets.mjs). The pack is used in place:
// desktop reads the file into memory, the device memory-maps the `assets` flash partition, and
// every lv_image_dsc_t points straight into those bytes (no copies, no decoding).
//
// Layout (little endian): "SJAR", u32 version=1, u32 count, u32 total bytes, u8 signature[16],
// then `count` 52-byte entries {char name[24], u32 format, u16 w, u16 h, u32 stride,
// f32 origin_x, f32 origin_y, u32 offset, u32 size}, then 4-byte-aligned pixel data.
// Formats: 1 = RGB565, 2 = RGB565A8 (colour plane then alpha plane), 3 = A8.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lvgl.h"

namespace sound_jar {

struct Sprite {
  lv_image_dsc_t dsc;
  float ox, oy;  // reference drawing origin inside the sprite (pivot)
  char name[24];
};

class AssetPack {
 public:
  static constexpr uint32_t kVersion = 1;
  // `data` must outlive the pack. Returns false and fills `error` on any inconsistency.
  bool load(const uint8_t* data, std::size_t size, std::string* error);
  const Sprite* find(const char* name) const;  // binary search (entries are indexed by name)
  const uint8_t* signature() const { return signature_; }
  uint32_t bytes() const { return bytes_; }
  std::size_t count() const { return sprites_.size(); }

 private:
  std::vector<Sprite> sprites_;  // sorted by name
  const uint8_t* signature_ = nullptr;
  uint32_t bytes_ = 0;
};

}  // namespace sound_jar
