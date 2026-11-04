#pragma once
#include <cstdint>

#include "little_arcade/feedback.hpp"
#include "little_arcade/input.hpp"
#include "little_arcade/rng.hpp"

namespace little_arcade {

// Services available to an active scene. Owned by the SceneManager and valid
// from enter() until exit() returns.
struct GameContext {
  GameContext(Rng& rng_in, IFeedback& feedback_in) : rng(rng_in), feedback(feedback_in) {}

  Rng& rng;                     // deterministic; seeded per scene entry
  IFeedback& feedback;          // sound/prompt requests; may be a NullFeedback
  bool audio_enabled = false;   // startup muted until configured (docs/acceptance.md G4)
  bool voice_enabled = false;   // push-to-talk opt-in; touch is always complete
};

// Scene lifecycle contract (docs/architecture.md). FROZEN for game work:
// changes need platform + integrator review.
//
// Rules for implementations:
// - Only the UI task calls these methods; no locking needed.
// - Navigation (Home button, BACK, PAUSE/RESUME, sleep) is handled by the
//   SceneManager before handle() is called. Games never see BACK/PAUSE/RESUME.
// - handle() receives touch events (x, y in landscape 320x240) and voice
//   events already filtered to active_contexts().
// - No heap allocation after enter(); no blocking; no failure state.
class IGame {
 public:
  virtual ~IGame() = default;
  virtual void enter(GameContext& context) = 0;
  virtual void update(uint32_t dt_ms) = 0;
  virtual void handle(const InputEvent& event) = 0;
  virtual void exit() = 0;
  // Bitmask of Context values whose intents this scene accepts.
  virtual uint32_t active_contexts() const = 0;
  virtual void on_pause() {}
  virtual void on_resume() {}
};

}  // namespace little_arcade
