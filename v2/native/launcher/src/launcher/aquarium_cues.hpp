#pragma once
// Bubble Aquarium's sound cues on the shared AudioFeedback synth. Same mapping as the standalone
// device image (v2/native/bubble_aquarium/device/main/main.cpp); used by the launcher on both
// device and desktop so they sound alike.
#include <cstdint>

#include "game/aquarium.hpp"
#include "little_arcade/feedback.hpp"

namespace launcher {

class AquariumCues final : public aquarium::Audio {
 public:
  explicit AquariumCues(little_arcade::IFeedback& sound) : sound_(sound) {}
  void play(aquarium::Cue cue, int variant) override {
    using little_arcade::Cue;
    if (!playing_) return;
    const auto v = static_cast<uint8_t>(variant);
    switch (cue) {
      case aquarium::Cue::Welcome: sound_.play(Cue::PROMPT, 0); break;
      case aquarium::Cue::Feed: sound_.play(Cue::TAP, v); break;
      case aquarium::Cue::Plip: sound_.play(Cue::TAP, 0); break;
      case aquarium::Cue::Gulp: sound_.play(Cue::SUCCESS, v); break;
      case aquarium::Cue::Happy: sound_.play(Cue::REACTION, v); break;
      case aquarium::Cue::Bubble: sound_.play(Cue::NOTE, 4); break;
      case aquarium::Cue::Pop: sound_.play(Cue::NOTE, 3); break;
      case aquarium::Cue::Glass: sound_.play(Cue::NOTE, 2); break;
      case aquarium::Cue::Fish: sound_.play(Cue::REACTION, v); break;
      case aquarium::Cue::Visitor: sound_.play(Cue::NOTE, static_cast<uint8_t>(7 + variant)); break;
      case aquarium::Cue::Bed: sound_.play(Cue::NOTE, static_cast<uint8_t>(5 + variant)); break;
      case aquarium::Cue::Hint: sound_.play(Cue::PROMPT, 0); break;
    }
  }
  void bedtime(bool) override {}
  void playing(bool on) override { playing_ = on; }

 private:
  little_arcade::IFeedback& sound_;
  bool playing_ = false;
};

}  // namespace launcher
