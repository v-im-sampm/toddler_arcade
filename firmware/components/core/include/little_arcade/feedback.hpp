#pragma once
#include <cstdint>

namespace little_arcade {

// Semantic sound/prompt requests from game logic. The audio layer maps
// (game, cue, variant) to a manifest asset. Every cue must also have a visual
// counterpart in the scene, because audio may be disabled.
enum class Cue : uint8_t {
  TAP,           // acknowledge any touch
  PROMPT,        // spoken/sung request (variant = prompt index, e.g. colour)
  SUCCESS,       // correct or completed action
  TRY_AGAIN,     // gentle, non-punitive nudge; never a buzzer
  CELEBRATE,     // short celebration jingle
  NOTE,          // musical/pitched sound (variant = note or sound index)
  REACTION,      // character reaction (variant = reaction index)
  AMBIENT,       // calm background loop start
};

class IFeedback {
 public:
  // Scene marker for set_scene(): the launcher, not a game.
  static constexpr uint8_t kSceneMenu = 0xFF;

  virtual ~IFeedback() = default;
  virtual void play(Cue cue, uint8_t variant) = 0;
  // Which scene subsequent cues belong to, so the audio layer can pick
  // that scene's sound palette.
  virtual void set_scene(uint8_t scene) { (void)scene; }
};

class NullFeedback final : public IFeedback {
 public:
  void play(Cue, uint8_t) override {}
};

}  // namespace little_arcade
