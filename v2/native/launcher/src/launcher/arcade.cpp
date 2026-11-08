#include "launcher/arcade.hpp"

#include <algorithm>
#include <cstdio>

#ifdef ESP_PLATFORM
#include "esp_partition.h"
#include "sprite_pack.hpp"  // v2/native/bubble_aquarium/device/main (generated with the Aquarium pack)
#endif

namespace launcher {
namespace {
constexpr double kStartX = 160, kStartY = 120;  // both games' Play button

// One Aquarium sprite for the menu tiles, from the same source the Aquarium view reads.
bool load_aquarium_sprite(aquarium::ArgbImage& img, const std::string& dir, const char* name, int w, int h,
                          std::string* error) {
  if (!img.allocate(w, h)) { *error = std::string("no memory for Aquarium sprite ") + name; return false; }
  const std::size_t bytes = static_cast<std::size_t>(w) * h * 4;
#ifdef ESP_PLATFORM
  (void)dir;
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets");
  if (!part) { *error = "no Aquarium 'assets' partition"; return false; }
  for (const auto& e : aquarium::pack::entries) {
    if (std::string(e.name) != name) continue;
    if (e.width != w || e.height != h || e.offset + bytes > part->size ||
        esp_partition_read(part, e.offset, img.pixels(), bytes) != ESP_OK) {
      *error = std::string("bad Aquarium sprite ") + name;
      return false;
    }
    return true;
  }
  *error = std::string("Aquarium sprite missing: ") + name;
  return false;
#else
  const std::string file = dir + "/" + name + ".bgra";
  FILE* f = std::fopen(file.c_str(), "rb");
  if (!f) { *error = "cannot open " + file; return false; }
  const bool ok = std::fread(img.pixels(), 1, bytes, f) == bytes && std::fgetc(f) == EOF;
  std::fclose(f);
  if (!ok) *error = "size mismatch " + file;
  return ok;
#endif
}

lv_obj_t* new_screen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}
}  // namespace

Arcade::Arcade(little_arcade::IFeedback& feedback, uint32_t seed)
    : feedback_(feedback), aq_cues_(feedback), cp_(cp_score_, seed), aq_(aq_cues_, seed) {}

bool Arcade::create_menu(const color_pop::AssetPack& pack, const std::string& aquarium_dir, std::string* error) {
  if (!load_aquarium_sprite(aq_backdrop_, aquarium_dir, "backdrop", 320, 240, error) ||
      !load_aquarium_sprite(aq_fish_, aquarium_dir, "fish_0_right_happy_0", 96, 80, error) ||
      !load_aquarium_sprite(aq_bubble_, aquarium_dir, "bubble", 50, 50, error))
    return false;
  auto cp = [&](const char* name) -> const lv_image_dsc_t* {
    const color_pop::Sprite* s = pack.find(name);
    return s ? &s->dsc : nullptr;
  };
  MenuArt art;
  art.cp_bg = cp("bg");
  art.cp_bubbles = {cp("bubble_red"), cp("bubble_yellow"), cp("bubble_blue")};
  art.cp_crab = cp("crab_body_open_happy");
  art.aq_backdrop = aq_backdrop_.dsc();
  art.aq_fish = aq_fish_.dsc();
  art.aq_bubble = aq_bubble_.dsc();
  menu_screen_ = new_screen();
  if (!menu_.create(menu_screen_, art)) { *error = "Color Pop menu sprites missing from pack"; return false; }
  menu_.sync(shell_);
  lv_screen_load(menu_screen_);
  return true;
}

bool Arcade::create_games(const color_pop::AssetPack& pack, const std::string& aquarium_dir, std::string* error) {
  cp_screen_ = new_screen();
  const char* missing = nullptr;
  if (!cp_view_.create(cp_screen_, pack, &missing)) {
    *error = std::string("Color Pop sprite missing: ") + (missing ? missing : "?");
    return false;
  }
  cp_view_.sync(cp_);
  aq_screen_ = new_screen();
  aq_view_.create(aq_screen_, aquarium_dir);  // aborts with a message on invalid assets
  aq_view_.sync(aq_);
  return true;
}

void Arcade::launch(Scene scene) {
  if (scene == Scene::ColorPop) {
    cp_.home();
    gate_.set_open(true);
    lv_screen_load(cp_screen_);
    cp_.down(kStartX, kStartY);  // the menu tap was the Play tap
    cp_view_.sync(cp_);
  } else if (scene == Scene::Aquarium) {
    aq_.reset();
    lv_screen_load(aq_screen_);
    aq_.down(static_cast<float>(kStartX), static_cast<float>(kStartY));
    aq_.up();
    aq_view_.sync(aq_);
  }
}

void Arcade::leave(Scene from) {
  if (from == Scene::ColorPop) {
    cp_.home();
    gate_.set_open(false);  // fades the waves out over 80 ms
    cp_view_.sync(cp_);
  } else if (from == Scene::Aquarium) {
    aq_.up();
    aq_.reset();
    aq_cues_.playing(false);
    aq_view_.sync(aq_);
  }
  lv_screen_load(menu_screen_);
  menu_.sync(shell_);
}

void Arcade::down(double x, double y) {
  const Scene before = shell_.scene();
  forwarding_ = false;
  switch (shell_.down(x, y)) {
    case Event::Pressed:
      feedback_.play(little_arcade::Cue::NOTE, static_cast<uint8_t>(shell_.pressed() == 0 ? 0 : 4));
      break;
    case Event::Home:
      feedback_.play(little_arcade::Cue::TAP, 0);
      leave(before);
      break;
    case Event::Forward:
      forwarding_ = true;
      if (before == Scene::ColorPop) cp_.down(x, y);
      else aq_.down(static_cast<float>(x), static_cast<float>(y));
      break;
    default:
      break;
  }
}

void Arcade::move(double x, double y) {
  if (forwarding_ && shell_.scene() == Scene::Aquarium) aq_.move(static_cast<float>(x), static_cast<float>(y));
}

void Arcade::up() {
  if (forwarding_ && shell_.scene() == Scene::Aquarium) aq_.up();
  forwarding_ = false;
}

void Arcade::frame(double dt) {
  dt = std::clamp(dt, 0.0, 0.25);
  if (shell_.tick(dt) == Event::Launch) launch(shell_.scene());
  switch (shell_.scene()) {
    case Scene::Menu:
      menu_.sync(shell_);
      break;
    case Scene::ColorPop:
      if (dt > 0) cp_.tick(std::min(0.05, dt));  // reference frame(): dt capped at 50 ms
      cp_view_.sync(cp_);
      break;
    case Scene::Aquarium:
      aq_.tick(static_cast<float>(dt));
      aq_view_.sync(aq_);
      break;
  }
}

}  // namespace launcher
