#pragma once
#include <cstdint>

#include "little_arcade/audio/synth.hpp"
#include "little_arcade/feedback.hpp"

namespace little_arcade::audio {

// Procedural, original sound design (docs/assets.md: synthetic tones, no
// copyrighted or character samples). The one exception is Animal Sounds,
// whose NOTE cue plays short public-domain/CC0 animal recordings with
// provenance in assets/audio/animal_sounds/sources.json. Gentle by
// construction: no buzzers, soft attacks, TRY_AGAIN is a quiet falling "hmm",
// nothing longer than ~1.2 s.
//
// `scene` is a GameId value or IFeedback::kSceneMenu. Games without a custom
// palette use the shared defaults. An empty() Sound means silence.
Sound sound_for(uint8_t scene, Cue cue, uint8_t variant);

// Played when the SceneManager moves between the launcher and a game.
Sound scene_change_sound(uint8_t from_scene, uint8_t to_scene);

// Longest sound the bank may return; used by tests and the mixer budget.
inline constexpr uint32_t kMaxSoundMs = 1200;
// Loudest per-tone level the bank uses (leaves mix headroom).
inline constexpr uint8_t kMaxToneLevel = 200;

}  // namespace little_arcade::audio
