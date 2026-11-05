#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/audio/synth.hpp"

namespace little_arcade::audio {

// Recorded animal calls for Animal Sounds (G06): public-domain/CC0 recordings
// listed with their licences in assets/audio/animal_sounds/sources.json,
// processed by tools/make_animal_clips.py. The table is generated at configure
// time by animal_clips.cmake and lives in flash.
inline constexpr std::size_t kAnimalClipCount = 8;

// Order matches animal_sounds::Animal: cow, duck, sheep, pig, dog, cat, frog, owl.
const PcmClip& animal_clip(uint8_t animal);

}  // namespace little_arcade::audio
