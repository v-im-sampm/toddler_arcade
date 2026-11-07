#include "ui/aquarium_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <utility>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "sprite_pack.hpp"
#endif

namespace aquarium {
namespace {
constexpr float tau = 6.28318530718f;
[[noreturn]] void fail(const std::string& reason) {
  std::fprintf(stderr, "Aquarium asset error: %s\n", reason.c_str());
  std::abort();
}
void show(lv_obj_t* o, bool yes) {
  if (yes) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t* veil(lv_obj_t* parent, int y, int h) {
  auto* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(o, 0, y); lv_obj_set_size(o, 320, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x1E2245), 0);
  lv_obj_set_style_bg_opa(o, 0, 0);
  return o;
}
std::string numbered(const std::string& prefix, int frame) {
  return prefix + "_" + std::to_string(frame);
}
int frame(float time, float speed) {
  return static_cast<int>(std::floor(time * speed * 6 / tau)) % 6;
}
#ifdef ESP_PLATFORM
const esp_partition_t* sprite_partition = nullptr;
std::string stored_name(std::string name) {
  const bool animated = name.rfind("fish_", 0) == 0 || name.rfind("visitor_", 0) == 0 ||
                        name.rfind("weed_", 0) == 0 || name.rfind("rays_", 0) == 0 ||
                        name.rfind("caustics_", 0) == 0 || name.rfind("surface_", 0) == 0 ||
                        name.rfind("button_", 0) == 0;
  if (animated && !name.empty() && (name.back() == '1' || name.back() == '3' || name.back() == '5'))
    name.back() = static_cast<char>(name.back() - 1);
  return name;
}
#endif
}
void View::load(const std::string& name, int w, int h) {
  const std::string canonical =
#ifdef ESP_PLATFORM
      stored_name(name);
  auto existing = sprites_.find(canonical);
  if (existing != sprites_.end()) {
    sprites_.emplace(name, Sprite{existing->second.image, w, h});
    return;
  }
#else
      name;
#endif
  auto img = std::make_shared<little_arcade::ui::ArgbImage>();
  if (!img->allocate(w, h)) fail("Cannot allocate sprite " + name);
#ifdef ESP_PLATFORM
  if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < static_cast<size_t>(w) * h * 4)
    fail("Not enough contiguous PSRAM for sprite " + canonical);
  const pack::Entry* entry = nullptr;
  for (const auto& candidate : pack::entries)
    if (canonical == candidate.name) { entry = &candidate; break; }
  if (!entry || entry->width != w || entry->height != h ||
      entry->offset + static_cast<uint32_t>(w * h * 4) > sprite_partition->size)
    fail("Invalid device sprite entry: " + canonical);
  if (esp_partition_read(sprite_partition, entry->offset, img->canvas().px,
                         static_cast<size_t>(w) * h * 4) != ESP_OK)
    fail("Device sprite read failed: " + canonical);
#else
  const std::string file = directory_ + "/" + name + ".bgra";
  FILE* f = std::fopen(file.c_str(), "rb");
  if (!f) fail("Cannot open sprite " + file);
  auto canvas = img->canvas();
  const std::size_t count = static_cast<std::size_t>(w) * h * 4;
  const bool valid = std::fread(canvas.px, 1, count, f) == count && std::fgetc(f) == EOF;
  std::fclose(f);
  if (!valid) fail("Sprite length mismatch: " + file);
#endif
  sprites_.emplace(canonical, Sprite{img, w, h});
  if (canonical != name) sprites_.emplace(name, Sprite{img, w, h});
}
lv_obj_t* View::object(lv_obj_t* parent, const std::string& sprite, int x, int y) {
  const auto it = sprites_.find(sprite);
  if (it == sprites_.end()) fail("Missing sprite: " + sprite);
  auto* o = lv_image_create(parent);
  lv_image_set_src(o, it->second.image->dsc());
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(o, x, y);
  return o;
}
void View::set(lv_obj_t* o, std::string& previous, const std::string& sprite) {
  if (previous == sprite) return;
  auto it = sprites_.find(sprite);
  if (it == sprites_.end()) fail("Missing frame: " + sprite);
  lv_image_set_src(o, it->second.image->dsc());
  previous = sprite;
}
void View::create(lv_obj_t* parent, const std::string& asset_directory) {
  directory_ = asset_directory;
#ifdef ESP_PLATFORM
  sprite_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets");
  if (!sprite_partition || pack::bytes > sprite_partition->size)
    fail("Aquarium assets partition missing or too small");
  uint8_t signature[sizeof(pack::signature)]{};
  if (esp_partition_read(sprite_partition, 0, signature, sizeof(signature)) != ESP_OK ||
      !std::equal(std::begin(signature), std::end(signature), std::begin(pack::signature)))
    fail("Aquarium sprite partition does not match this firmware image");
#endif
  load("backdrop", 320, 240); load("start", 320, 240);
  load("chrome_home", 48, 48); load("chrome_mic", 48, 48);
  load("bubble", 50, 50); load("pellet", 24, 24);
  load("shadow", 60, 22); load("helper_hand", 36, 36);
  for (int i = 0; i < 4; ++i) load(numbered("chest", i), 64, 56);
  for (int f = 0; f < 6; ++f) {
    load(numbered("rays", f), 320, 144);
    load(numbered("caustics", f), 320, 36);
    load(numbered("surface", f), 320, 14);
    for (const auto& layer : {"back_mid", "back_right", "front_left", "front_right"}) {
      int w = 0, h = 0;
      const std::string n = layer;
      if (n == "back_mid") { w = 62; h = 80; }
      if (n == "back_right") { w = 42; h = 80; }
      if (n == "front_left") { w = 55; h = 100; }
      if (n == "front_right") { w = 36; h = 75; }
      load(numbered("weed_" + n, f), w, h);
    }
    for (int i = 0; i < 3; ++i) {
      for (const auto& face : {"left", "right"})
        for (const auto& mood : {"normal", "happy", "sleep"})
          load(numbered("fish_" + std::to_string(i) + "_" + face + "_" + mood, f), 96, 80);
      load(numbered("visitor_" + std::to_string(i), f), 112, 88);
    }
    for (int night = 0; night < 2; ++night)
      for (const auto& button : {"shaker", "moon", "bubbles", "hint"})
        load(numbered("button_" + std::string(button) + "_" + std::to_string(night), f), 48, 48);
  }
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  backdrop_ = object(parent, "backdrop", 0, 0);
  rays_ = object(parent, "rays_0", 0, 62);
  caustics_ = object(parent, "caustics_0", 0, 204);
  weed_[0] = object(parent, "weed_back_mid_0", 116, 140);
  weed_[1] = object(parent, "weed_back_right_0", 278, 140);
  chest_ = object(parent, "chest_0", 188, 184);
  dim_ = veil(parent, 0, 240);
  for (auto& o : pellets_) o = object(parent, "pellet", 0, 0);
  for (int i = 0; i < 3; ++i) {
    shadows_[i] = object(parent, "shadow", 0, 212);
    fish_[i] = object(parent, "fish_" + std::to_string(i) + "_right_normal_0", 0, 0);
  }
  visitor_ = object(parent, "visitor_0_0", -200, 0);
  for (auto& o : bubbles_) o = object(parent, "bubble", 0, 0);
  weed_[2] = object(parent, "weed_front_left_0", 0, 120);
  weed_[3] = object(parent, "weed_front_right_0", 284, 145);
  surface_ = object(parent, "surface_0", 0, 58);
  foreground_dim_ = veil(parent, 0, 240);
  hint_hand_ = object(parent, "helper_hand", 90, 62);
  for (int i = 0; i < 3; ++i)
    buttons_[i] = object(parent, "button_shaker_0_0", 76 + i * 60, 4);
  object(parent, "chrome_home", 4, 4);
  object(parent, "chrome_mic", 268, 4);
  start_ = object(parent, "start", 0, 0);
}
void View::sync(const Game& game) {
  const float t = game.time();
  const int weed_phase = frame(t, 1.1f * (game.reduced() ? 0.3f : 1));
  const char* layers[] = {"back_mid", "back_right", "front_left", "front_right"};
  for (int i = 0; i < 4; ++i)
    set(weed_[i], weed_frame_[i], numbered("weed_" + std::string(layers[i]), weed_phase));
  set(rays_, rays_frame_, numbered("rays", frame(t, 0.23f)));
  set(caustics_, caustics_frame_, numbered("caustics", frame(t, 0.5f)));
  set(surface_, surface_frame_, numbered("surface", frame(t, 1.2f)));
  lv_obj_set_style_bg_opa(dim_, static_cast<lv_opa_t>(game.dim() * 110), 0);
  lv_obj_set_style_bg_opa(foreground_dim_, static_cast<lv_opa_t>(game.dim() * 44), 0);
  set(chest_, chest_frame_, numbered("chest", game.bubble_count() > 0 && game.bubbles()[game.bubble_count() - 1].age < 1.2f ? 3 : 0));
  for (int i = 0; i < 6; ++i) {
    show(pellets_[i], i < game.pellet_count());
    if (i < game.pellet_count()) {
      const auto& p = game.pellets()[i];
      lv_obj_set_pos(pellets_[i], static_cast<int>(p.x) - 12, static_cast<int>(p.y) - 12);
    }
  }
  for (int i = 0; i < 3; ++i) {
    const auto& f = game.fish()[i];
    const std::string mood = f.mode == Mode::Sleep && f.asleep && f.peek <= 0 ? "sleep" :
                             f.happy > 0 ? "happy" : "normal";
    const std::string sprite = "fish_" + std::to_string(i) + "_" + (f.face > 0 ? "right" : "left") +
                               "_" + mood + "_" + std::to_string(frame(f.tail, 1));
    set(fish_[i], fish_frame_[i], sprite);
    lv_obj_set_pos(fish_[i], static_cast<int>(f.x) - 48,
                   static_cast<int>(f.y + std::sin(t * 1.3f + i) * (f.asleep ? 2 : 1)) - 40);
    lv_obj_set_pos(shadows_[i], static_cast<int>(f.x) - 30, 212);
  }
  const auto& v = game.visitor();
  show(visitor_, v.active);
  if (v.active) {
    set(visitor_, visitor_frame_, "visitor_" + std::to_string(v.kind) + "_" + std::to_string(frame(v.age, 1.3f)));
    lv_obj_set_pos(visitor_, static_cast<int>(v.x) - 56, static_cast<int>(v.y) - 44);
    lv_image_set_rotation(visitor_, static_cast<int32_t>(std::sin(v.wave * 14) * v.wave * 150));
  }
  for (int i = 0; i < 8; ++i) {
    show(bubbles_[i], i < game.bubble_count());
    if (i < game.bubble_count()) {
      const auto& b = game.bubbles()[i];
      lv_obj_set_pos(bubbles_[i], static_cast<int>(b.x) - 25, static_cast<int>(b.y) - 25);
      lv_image_set_scale(bubbles_[i], static_cast<uint16_t>(b.radius * b.growth * 256 / 20));
    }
  }
  show(hint_hand_, game.hint() >= 2 && !game.bed());
  for (int i = 0; i < 3; ++i) {
    const std::string name = i == 0 ? (game.hint() > 0 && !game.bed() ? "hint" : "shaker") :
                             i == 1 ? "moon" : "bubbles";
    set(buttons_[i], button_frame_[i], "button_" + name + "_" + (game.bed() ? "1" : "0") + "_" +
                                      std::to_string(static_cast<int>(t / 0.15f) % 6));
  }
  show(start_, game.phase() == Phase::Start);
}
}
