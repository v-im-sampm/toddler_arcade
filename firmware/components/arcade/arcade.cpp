#include "little_arcade/arcade.hpp"

#include "little_arcade/games/animal_sounds_art.hpp"
#include "little_arcade/games/beat_sharks_art.hpp"
#include "little_arcade/games/color_pop_art.hpp"
#include "little_arcade/games/feed_monster_art.hpp"
#include "little_arcade/games/hatch_a_pet_art.hpp"
#include "little_arcade/games/rainbow_piano_art.hpp"
#include "little_arcade/games/robo_buddy_art.hpp"
#include "little_arcade/games/shape_garage_art.hpp"
#include "little_arcade/games/sound_jar_art.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade {

namespace {
namespace pal = ui::palette;

// Launcher tile pictures, painted once in create(). TileArtFn is a plain
// function pointer, so the images live at file scope.
ui::ArgbImage g_color_pop_tile;
ui::ArgbImage g_animal_sounds_tile;
ui::ArgbImage g_hatch_a_pet_tile;
ui::ArgbImage g_rainbow_piano_tile;
ui::ArgbImage g_robo_buddy_tile;
ui::ArgbImage g_shape_garage_tile;
ui::ArgbImage g_feed_monster_tile;
ui::ArgbImage g_sound_jar_tile;
ui::ArgbImage g_beat_sharks_tile;

ui::TileArt tile_art(GameId id) {
  switch (id) {
    case GameId::COLOR_POP:
      return {g_color_pop_tile.ready() ? g_color_pop_tile.dsc() : nullptr, pal::kTilePop};
    case GameId::ANIMAL_SOUNDS:
      return {g_animal_sounds_tile.ready() ? g_animal_sounds_tile.dsc() : nullptr, 0xDDF3E8};
    case GameId::HATCH_A_PET:
      return {g_hatch_a_pet_tile.ready() ? g_hatch_a_pet_tile.dsc() : nullptr, pal::kTileEgg};
    case GameId::RAINBOW_PIANO:
      return {g_rainbow_piano_tile.ready() ? g_rainbow_piano_tile.dsc() : nullptr, pal::kTilePiano};
    case GameId::ROBO_BUDDY:
      return {g_robo_buddy_tile.ready() ? g_robo_buddy_tile.dsc() : nullptr, 0xD4F3F0};
    case GameId::SHAPE_GARAGE:
      return {g_shape_garage_tile.ready() ? g_shape_garage_tile.dsc() : nullptr, 0xD4F3F0};
    case GameId::FEED_MONSTER:
      return {g_feed_monster_tile.ready() ? g_feed_monster_tile.dsc() : nullptr, 0xEFE2FF};
    case GameId::SOUND_JAR:
      return {g_sound_jar_tile.ready() ? g_sound_jar_tile.dsc() : nullptr, 0xFFE3EE};
    case GameId::BEAT_SHARKS:
      return {g_beat_sharks_tile.ready() ? g_beat_sharks_tile.dsc() : nullptr, 0xD6F0FA};
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

Arcade::Arcade(IFeedback& feedback, uint32_t seed, uint8_t volume_level, bool mic_enabled)
    : entries_{{GameId::COLOR_POP, Intent::COLORS, &color_pop_},
                {GameId::HATCH_A_PET, Intent::EGGS, &hatch_a_pet_},
                {GameId::RAINBOW_PIANO, Intent::MUSIC, &rainbow_piano_},
                {GameId::ROBO_BUDDY, Intent::ROBOT, &robo_buddy_},
                {GameId::SHAPE_GARAGE, Intent::SHAPES, &shape_garage_},
                {GameId::ANIMAL_SOUNDS, Intent::ANIMALS, &animal_sounds_},
                {GameId::FEED_MONSTER, Intent::COUNTING, &feed_monster_},
                // No spoken launcher name: kids open Sound Jar from its tile.
                {GameId::SOUND_JAR, Intent::UNKNOWN, &sound_jar_},
                {GameId::BEAT_SHARKS, Intent::UNKNOWN, &beat_sharks_}},
      feedback_(feedback),
      scenes_(entries_, kGameCount, feedback, config_for(seed)),
      gate_(seed, volume_level, mic_enabled) {
  scenes_.set_audio_enabled(gate_.level() > 0);
}

void Arcade::create(lv_obj_t* screen) {
  if (g_animal_sounds_tile.allocate(animal_sounds::art::kTileW, animal_sounds::art::kTileH)) {
    ui::Canvas c = g_animal_sounds_tile.canvas();
    animal_sounds::art::paint_tile_art(c);
  }
  if (g_color_pop_tile.allocate(color_pop::kTileW, color_pop::kTileH)) {
    ui::Canvas c = g_color_pop_tile.canvas();
    color_pop::paint_tile_art(c);
  }
  if (g_hatch_a_pet_tile.allocate(hatch_a_pet::art::kTileW, hatch_a_pet::art::kTileH)) {
    ui::Canvas c = g_hatch_a_pet_tile.canvas();
    hatch_a_pet::art::paint_tile_art(c);
  }
  if (g_rainbow_piano_tile.allocate(rainbow_piano::art::kTileW, rainbow_piano::art::kTileH)) {
    ui::Canvas c = g_rainbow_piano_tile.canvas();
    rainbow_piano::art::paint_tile_art(c);
  }
  if (g_robo_buddy_tile.allocate(robo_buddy::art::kTileW, robo_buddy::art::kTileH)) {
    ui::Canvas c = g_robo_buddy_tile.canvas();
    robo_buddy::art::paint_tile_art(c);
  }
  if (g_shape_garage_tile.allocate(shape_garage::art::kTileW, shape_garage::art::kTileH)) {
    ui::Canvas c = g_shape_garage_tile.canvas();
    shape_garage::art::paint_tile_art(c);
  }
  if (g_feed_monster_tile.allocate(feed_monster::art::kTileW, feed_monster::art::kTileH)) {
    ui::Canvas c = g_feed_monster_tile.canvas();
    feed_monster::art::paint_tile_art(c);
  }
  if (g_sound_jar_tile.allocate(sound_jar::art::kTileW, sound_jar::art::kTileH)) {
    ui::Canvas c = g_sound_jar_tile.canvas();
    sound_jar::art::paint_tile_art(c);
  }
  if (g_beat_sharks_tile.allocate(beat_sharks::art::kTileW, beat_sharks::art::kTileH)) {
    ui::Canvas c = g_beat_sharks_tile.canvas();
    beat_sharks::art::paint_tile_art(c);
  }

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 1; i < 11; ++i) layers_[i] = full_screen(screen);  // [0] = Layer::NONE
  launcher_.create(layers_[static_cast<int>(Layer::MENU)], tile_art);
  animal_sounds_view_.create(layers_[static_cast<int>(Layer::ANIMAL_SOUNDS)]);
  color_view_.create(layers_[static_cast<int>(Layer::COLOR_POP)]);
  hatch_view_.create(layers_[static_cast<int>(Layer::HATCH_A_PET)]);
  rainbow_piano_view_.create(layers_[static_cast<int>(Layer::RAINBOW_PIANO)]);
  robo_buddy_view_.create(layers_[static_cast<int>(Layer::ROBO_BUDDY)]);
  shape_garage_view_.create(layers_[static_cast<int>(Layer::SHAPE_GARAGE)]);
  feed_monster_view_.create(layers_[static_cast<int>(Layer::FEED_MONSTER)]);
  sound_jar_view_.create(layers_[static_cast<int>(Layer::SOUND_JAR)]);
  beat_sharks_view_.create(layers_[static_cast<int>(Layer::BEAT_SHARKS)]);
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
  // Sound Jar needs the held/sliding finger too (looping and pitch bend).
  if (!gate_.open() && sound_jar_.active() && scenes_.mode() == SceneManager::Mode::PLAYING) {
    sound_jar_.pointer(pressed, x, y);
  }
}

void Arcade::touch(int16_t x, int16_t y, uint32_t now_ms) { scenes_.handle(make_touch_event(x, y, now_ms)); }

void Arcade::voice(Intent intent, uint32_t now_ms) { scenes_.handle(make_voice_event(intent, now_ms)); }

Arcade::Layer Arcade::wanted_layer() const {
  const GameEntry* cur = scenes_.current();
  if (scenes_.mode() == SceneManager::Mode::MENU || cur == nullptr) return Layer::MENU;
  switch (cur->id) {
    case GameId::COLOR_POP: return Layer::COLOR_POP;
    case GameId::ANIMAL_SOUNDS: return Layer::ANIMAL_SOUNDS;
    case GameId::HATCH_A_PET: return Layer::HATCH_A_PET;
    case GameId::RAINBOW_PIANO: return Layer::RAINBOW_PIANO;
    case GameId::ROBO_BUDDY: return Layer::ROBO_BUDDY;
    case GameId::SHAPE_GARAGE: return Layer::SHAPE_GARAGE;
    case GameId::FEED_MONSTER: return Layer::FEED_MONSTER;
    case GameId::SOUND_JAR: return Layer::SOUND_JAR;
    case GameId::BEAT_SHARKS: return Layer::BEAT_SHARKS;
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
  if (layer == Layer::ANIMAL_SOUNDS) animal_sounds_view_.reset();
  if (layer == Layer::HATCH_A_PET) hatch_view_.reset();
  if (layer == Layer::RAINBOW_PIANO) rainbow_piano_view_.reset();
  if (layer == Layer::ROBO_BUDDY) robo_buddy_view_.reset();
  if (layer == Layer::SHAPE_GARAGE) shape_garage_view_.reset();
  if (layer == Layer::FEED_MONSTER) feed_monster_view_.reset();
  if (layer == Layer::SOUND_JAR) sound_jar_view_.reset();
  if (layer == Layer::BEAT_SHARKS) beat_sharks_view_.reset();
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
    case Layer::ANIMAL_SOUNDS: animal_sounds_view_.sync(animal_sounds_, now_ms); break;
    case Layer::COLOR_POP: color_view_.sync(color_pop_, now_ms); break;
    case Layer::HATCH_A_PET: hatch_view_.sync(hatch_a_pet_, now_ms); break;
    case Layer::RAINBOW_PIANO: rainbow_piano_view_.sync(rainbow_piano_); break;
    case Layer::ROBO_BUDDY: robo_buddy_view_.sync(robo_buddy_, now_ms); break;
    case Layer::SHAPE_GARAGE: shape_garage_view_.sync(shape_garage_, now_ms); break;
    case Layer::FEED_MONSTER: feed_monster_view_.sync(feed_monster_, now_ms); break;
    case Layer::SOUND_JAR: sound_jar_view_.sync(sound_jar_); break;
    case Layer::BEAT_SHARKS: beat_sharks_view_.sync(beat_sharks_); break;
    case Layer::NONE: break;
  }
  // Opening the grown-up gate drops anything recorded (privacy).
  const bool gate_open = gate_.open();
  if (gate_open && !gate_was_open_) {
    sound_jar_.privacy_wipe();
    beat_sharks_.privacy_wipe();
  }
  gate_was_open_ = gate_open;
  scenes_.set_audio_enabled(gate_.level() > 0);
  // Let the parent hear the level just chosen (the caller has applied it).
  if (gate_.take_preview()) feedback_.play(Cue::SUCCESS, 0);
  parent_view_.sync(gate_);
}

}  // namespace little_arcade
