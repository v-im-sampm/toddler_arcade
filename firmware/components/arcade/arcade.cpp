#include "little_arcade/arcade.hpp"

#include "little_arcade/games/color_pop_art.hpp"
#include "little_arcade/games/hatch_a_pet_art.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade {

namespace {
namespace pal = ui::palette;

// Launcher tile pictures, painted once in create(). TileArtFn is a plain
// function pointer, so the images live at file scope.
ui::ArgbImage g_color_pop_tile;
ui::ArgbImage g_hatch_a_pet_tile;

ui::TileArt tile_art(GameId id) {
  switch (id) {
    case GameId::COLOR_POP:
      return {g_color_pop_tile.ready() ? g_color_pop_tile.dsc() : nullptr, pal::kTilePop};
    case GameId::HATCH_A_PET:
      return {g_hatch_a_pet_tile.ready() ? g_hatch_a_pet_tile.dsc() : nullptr, pal::kTileEgg};
    default:
      return {};
  }
}

lv_obj_t* full_screen(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_size(o, kScreenWidth, kScreenHeight);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  return o;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

SceneManager::Config config_for(uint32_t seed) {
  SceneManager::Config c;
  c.base_seed = seed;
  return c;
}
}  // namespace

Arcade::Arcade(IFeedback& feedback, uint32_t seed, uint8_t volume_level)
    : entries_{{GameId::COLOR_POP, Intent::COLORS, &color_pop_}, {GameId::HATCH_A_PET, Intent::EGGS, &hatch_a_pet_}},
      feedback_(feedback),
      scenes_(entries_, kGameCount, feedback, config_for(seed)),
      gate_(seed, volume_level) {
  scenes_.set_audio_enabled(gate_.level() > 0);
}

void Arcade::create(lv_obj_t* screen) {
  if (g_color_pop_tile.allocate(color_pop::kTileW, color_pop::kTileH)) {
    ui::Canvas c = g_color_pop_tile.canvas();
    color_pop::paint_tile_art(c);
  }
  if (g_hatch_a_pet_tile.allocate(hatch_a_pet::art::kTileW, hatch_a_pet::art::kTileH)) {
    ui::Canvas c = g_hatch_a_pet_tile.canvas();
    hatch_a_pet::art::paint_tile_art(c);
  }

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 1; i < 4; ++i) layers_[i] = full_screen(screen);  // [0] = Layer::NONE
  launcher_.create(layers_[static_cast<int>(Layer::MENU)], tile_art);
  color_view_.create(layers_[static_cast<int>(Layer::COLOR_POP)]);
  hatch_view_.create(layers_[static_cast<int>(Layer::HATCH_A_PET)]);
  chrome_.create(screen);  // above every layer

  // Sleep: dim everything; the next tap wakes (and is consumed).
  sleep_overlay_ = full_screen(screen);
  lv_obj_set_style_bg_color(sleep_overlay_, ui::to_lv(pal::kInk), 0);
  lv_obj_set_style_bg_opa(sleep_overlay_, LV_OPA_70, 0);
  parent_view_.create(screen);  // topmost

  shown_ = Layer::NONE;
  sleeping_ = false;
  show_layer(wanted_layer());
}

void Arcade::pointer(bool pressed, int16_t x, int16_t y, uint32_t now_ms) {
  if (gate_.pointer(pressed, x, y, now_ms)) touch(x, y, now_ms);
}

void Arcade::touch(int16_t x, int16_t y, uint32_t now_ms) { scenes_.handle(make_touch_event(x, y, now_ms)); }

void Arcade::voice(Intent intent, uint32_t now_ms) { scenes_.handle(make_voice_event(intent, now_ms)); }

Arcade::Layer Arcade::wanted_layer() const {
  const GameEntry* cur = scenes_.current();
  if (scenes_.mode() == SceneManager::Mode::MENU || cur == nullptr) return Layer::MENU;
  switch (cur->id) {
    case GameId::COLOR_POP: return Layer::COLOR_POP;
    case GameId::HATCH_A_PET: return Layer::HATCH_A_PET;
    default: return Layer::MENU;
  }
}

void Arcade::show_layer(Layer layer) {
  if (layer == shown_) return;
  if (shown_ != Layer::NONE) set_visible(layers_[static_cast<int>(shown_)], false);
  shown_ = layer;
  set_visible(layers_[static_cast<int>(layer)], true);
  chrome_.set_home_visible(layer != Layer::MENU);
  // A (re)launched game restarts its animation timing.
  if (layer == Layer::COLOR_POP) color_view_.reset();
  if (layer == Layer::HATCH_A_PET) hatch_view_.reset();
}

void Arcade::tick(uint32_t now_ms) {
  const uint32_t dt = have_tick_ ? now_ms - last_ms_ : 0;
  have_tick_ = true;
  last_ms_ = now_ms;
  scenes_.update(dt);

  const bool sleeping = scenes_.mode() == SceneManager::Mode::SLEEPING;
  if (sleeping != sleeping_) {
    sleeping_ = sleeping;
    set_visible(sleep_overlay_, sleeping);
  }
  if (!sleeping) show_layer(wanted_layer());
  switch (shown_) {
    case Layer::MENU: launcher_.sync(scenes_); break;
    case Layer::COLOR_POP: color_view_.sync(color_pop_, now_ms); break;
    case Layer::HATCH_A_PET: hatch_view_.sync(hatch_a_pet_, now_ms); break;
    case Layer::NONE: break;
  }
  scenes_.set_audio_enabled(gate_.level() > 0);
  // Let the parent hear the level just chosen (the caller has applied it).
  if (gate_.take_preview()) feedback_.play(Cue::SUCCESS, 0);
  parent_view_.sync(gate_);
}

}  // namespace little_arcade
