#include "ui/asset_pack.hpp"

#include <cstring>

namespace color_pop {
namespace {
constexpr std::size_t kHeader = 32, kEntry = 52;
uint32_t u32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t u16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
float f32(const uint8_t* p) { float v; std::memcpy(&v, p, 4); return v; }
}  // namespace

bool AssetPack::load(const uint8_t* data, std::size_t size, std::string* error) {
  auto fail = [&](const std::string& why) { if (error) *error = why; sprites_.clear(); return false; };
  if (!data || size < kHeader || std::memcmp(data, "CPOP", 4) != 0) return fail("not a Color Pop pack");
  if (u32(data + 4) != kVersion) return fail("unsupported pack version");
  const uint32_t count = u32(data + 8);
  bytes_ = u32(data + 12);
  if (bytes_ > size || kHeader + kEntry * static_cast<std::size_t>(count) > bytes_) return fail("truncated pack");
  signature_ = data + 16;
  sprites_.clear();
  sprites_.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t* e = data + kHeader + kEntry * i;
    Sprite s{};
    std::memcpy(s.name, e, 23);
    const uint32_t format = u32(e + 24), w = u16(e + 28), h = u16(e + 30), stride = u32(e + 32);
    const uint32_t offset = u32(e + 44), bytes = u32(e + 48);
    if (static_cast<uint64_t>(offset) + bytes > bytes_ || (offset & 3u)) return fail(std::string("bad entry ") + s.name);
    lv_color_format_t cf;
    uint32_t expect;
    switch (format) {
      case 1: cf = LV_COLOR_FORMAT_RGB565; expect = stride * h; break;
      case 2: cf = LV_COLOR_FORMAT_RGB565A8; expect = stride * h + w * h; break;
      case 3: cf = LV_COLOR_FORMAT_A8; expect = stride * h; break;
      default: return fail(std::string("bad format in ") + s.name);
    }
    if (bytes != expect) return fail(std::string("size mismatch in ") + s.name);
    s.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s.dsc.header.cf = cf;
    s.dsc.header.w = w;
    s.dsc.header.h = h;
    s.dsc.header.stride = stride;
    s.dsc.data_size = bytes;
    s.dsc.data = data + offset;
    s.ox = f32(e + 36);
    s.oy = f32(e + 40);
    sprites_.push_back(s);
  }
  return true;
}

const Sprite* AssetPack::find(const char* name) const {
  for (const Sprite& s : sprites_)
    if (std::strncmp(s.name, name, sizeof s.name) == 0) return &s;
  return nullptr;
}

}  // namespace color_pop
