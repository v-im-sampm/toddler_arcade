#pragma once
#include <cstdint>

#include "little_arcade/games/animal_sounds.hpp"
#include "little_arcade/games/animal_sounds_view.hpp"
#include "little_arcade/games/beat_sharks.hpp"
#include "little_arcade/games/beat_sharks_view.hpp"
#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_view.hpp"
#include "little_arcade/games/feed_monster.hpp"
#include "little_arcade/games/feed_monster_view.hpp"
#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/games/hatch_a_pet_view.hpp"
#include "little_arcade/games/rainbow_piano.hpp"
#include "little_arcade/games/rainbow_piano_view.hpp"
#include "little_arcade/games/robo_buddy.hpp"
#include "little_arcade/games/robo_buddy_view.hpp"
#include "little_arcade/games/shape_garage.hpp"
#include "little_arcade/games/shape_garage_view.hpp"
#include "little_arcade/games/sound_jar.hpp"
#include "little_arcade/games/sound_jar_view.hpp"
#include "little_arcade/parent_view.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/settings/parent_gate.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/launcher_view.hpp"
#include "lvgl.h"

namespace little_arcade {

// The arcade shell shared by the device firmware and the desktop simulator:
// the games, the SceneManager (launcher, Home, sleep) and one LVGL layer per
// scene. Only the visible layer is synced each frame. Runs on the UI task.
class Arcade {
 public:
  enum class Layer : uint8_t {
    NONE,
    MENU,
    COLOR_POP,
    HATCH_A_PET,
    RAINBOW_PIANO,
    ROBO_BUDDY,
    SHAPE_GARAGE,
    ANIMAL_SOUNDS,
    FEED_MONSTER,
    SOUND_JAR,
    BEAT_SHARKS
  };

  // `volume_level` is the parent-saved level (0 = muted, the default);
  // `mic_enabled` is the parent-saved Sound Jar microphone switch.
  Arcade(IFeedback& feedback, uint32_t seed, uint8_t volume_level = 0, bool mic_enabled = true);
  Arcade(const Arcade&) = delete;
  Arcade& operator=(const Arcade&) = delete;

  // The shared sampler (owned by the audio component) for Sound Jar and Beat
  // Sharks. Attach before the first tick; without one both still run (Sound
  // Jar with picture-only pads, Beat Sharks with synth boings).
  void attach_sampler(ISampler* sampler) {
    sound_jar_.attach_sampler(sampler);
    beat_sharks_.attach_sampler(sampler);
  }

  // Builds every scene's objects once on `screen`.
  void create(lv_obj_t* screen);
  // Raw touch-panel state, sampled every UI loop: drives the hidden parent
  // gate (hold the Mic corner 5 s) and forwards new presses to touch().
  void pointer(bool pressed, int16_t x, int16_t y, uint32_t now_ms);
  // A press edge for the launcher/game (bench scripts call this directly).
  void touch(int16_t x, int16_t y, uint32_t now_ms);
  void voice(Intent intent, uint32_t now_ms);
  // Advances game time and syncs the visible view. Call once per UI loop.
  void tick(uint32_t now_ms);

  // Parent-chosen volume (0..6). The caller applies it to the audio output
  // and persists it when take_volume_changed() returns true.
  uint8_t volume_level() const { return gate_.level(); }
  bool take_volume_changed() { return gate_.take_level_changed(); }
  bool parent_gate_open() const { return gate_.open(); }
  // Parent Sound Jar microphone switch. The caller applies it to the sampler
  // and persists it when take_mic_changed() returns true.
  bool mic_enabled() const { return gate_.mic_enabled(); }
  bool take_mic_changed() { return gate_.take_mic_changed(); }

  const SceneManager& scenes() const { return scenes_; }
  Layer layer() const { return shown_; }
  const animal_sounds::AnimalSoundsGame& animal_sounds() const { return animal_sounds_; }
  const color_pop::ColorPopGame& color_pop() const { return color_pop_; }
  const feed_monster::FeedMonsterGame& feed_monster() const { return feed_monster_; }
  const feed_monster::FeedMonsterView& feed_monster_view() const { return feed_monster_view_; }
  const hatch_a_pet::HatchAPetGame& hatch_a_pet() const { return hatch_a_pet_; }
  const hatch_a_pet::HatchAPetView& hatch_a_pet_view() const { return hatch_view_; }
  const rainbow_piano::RainbowPianoGame& rainbow_piano() const { return rainbow_piano_; }
  const robo_buddy::RoboBuddyGame& robo_buddy() const { return robo_buddy_; }
  const shape_garage::ShapeGarageGame& shape_garage() const { return shape_garage_; }
  const sound_jar::SoundJarGame& sound_jar() const { return sound_jar_; }
  const beat_sharks::BeatSharksGame& beat_sharks() const { return beat_sharks_; }

 private:
  static constexpr std::size_t kGameCount = 9;

  Layer wanted_layer() const;
  void show_layer(Layer layer);

  animal_sounds::AnimalSoundsGame animal_sounds_;
  color_pop::ColorPopGame color_pop_;
  hatch_a_pet::HatchAPetGame hatch_a_pet_;
  rainbow_piano::RainbowPianoGame rainbow_piano_;
  robo_buddy::RoboBuddyGame robo_buddy_;
  shape_garage::ShapeGarageGame shape_garage_;
  feed_monster::FeedMonsterGame feed_monster_;
  sound_jar::SoundJarGame sound_jar_;
  beat_sharks::BeatSharksGame beat_sharks_;
  GameEntry entries_[kGameCount];
  IFeedback& feedback_;
  SceneManager scenes_;
  settings::ParentGate gate_;
  ParentView parent_view_;

  animal_sounds::AnimalSoundsView animal_sounds_view_;
  ui::LauncherView launcher_;
  color_pop::ColorPopView color_view_;
  hatch_a_pet::HatchAPetView hatch_view_;
  rainbow_piano::RainbowPianoView rainbow_piano_view_;
  robo_buddy::RoboBuddyView robo_buddy_view_;
  shape_garage::ShapeGarageView shape_garage_view_;
  feed_monster::FeedMonsterView feed_monster_view_;
  sound_jar::SoundJarView sound_jar_view_;
  beat_sharks::BeatSharksView beat_sharks_view_;
  ui::ChromeView chrome_;
  lv_obj_t* layers_[11]{};
  lv_obj_t* sleep_overlay_ = nullptr;
  Layer shown_ = Layer::NONE;
  bool sleeping_ = false;
  bool gate_was_open_ = false;
  bool have_tick_ = false;
  uint32_t last_ms_ = 0;
};

}  // namespace little_arcade
