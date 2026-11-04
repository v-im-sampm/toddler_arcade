#pragma once
#include <cstdint>

#include "little_arcade/input.hpp"

namespace little_arcade {

// Services a scene may use while active. Provisional: P02/P03 extend this with
// UI, audio and asset handles before the interface is frozen for game work.
struct GameContext {
  uint32_t rng_seed = 0;        // fixed in test mode for deterministic scenes
  bool audio_enabled = false;   // startup muted until configured (docs/acceptance.md G4)
  bool voice_enabled = false;   // push-to-talk opt-in; touch is always complete
};

// Scene lifecycle contract (docs/architecture.md). Only the UI task calls
// these methods, so implementations need no internal locking.
class IGame {
 public:
  virtual ~IGame() = default;
  virtual void enter(GameContext& context) = 0;
  virtual void update(uint32_t dt_ms) = 0;
  virtual void handle(const InputEvent& event) = 0;
  virtual void exit() = 0;
  // Contexts whose intents this scene accepts (bitmask of Context values).
  virtual uint32_t active_contexts() const = 0;
};

}  // namespace little_arcade
