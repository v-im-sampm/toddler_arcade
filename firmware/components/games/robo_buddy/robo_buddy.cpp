#include "little_arcade/games/robo_buddy.hpp"

namespace little_arcade::robo_buddy {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
uint32_t saturating_add(uint32_t a, uint32_t b) { return a > UINT32_MAX - b ? UINT32_MAX : a + b; }

// REACTION variants: Mood index for mood reactions, plus a wake-up chirp.
constexpr uint8_t kWakeReaction = static_cast<uint8_t>(kMoodCount);
// PROMPT variants.
constexpr uint8_t kPromptPlay = 0;   // "Hi! Pat me, tickle me or give me a snack!"
constexpr uint8_t kPromptWake = 1;   // "Zzz... tap me to wake me up!"
}  // namespace

void RoboBuddyGame::reset_state() {
  phase_ = Phase::INTRO;
  mood_ = Mood::CALM;
  last_action_ = Action::NONE;
  pose_ = 0;
  snack_ = Snack::BOLT_COOKIE;
  dancing_ = false;
  reactions_ = 0;
  naps_ = 0;
  moods_seen_ = 0;
  phase_ms_ = 0;
  hint_ms_ = 0;
  idle_ms_ = 0;
}

void RoboBuddyGame::enter(GameContext& context) {
  ctx_ = &context;
  reset_state();
  set_mood(Mood::CALM);
  snack_ = static_cast<Snack>(ctx_->rng.below(static_cast<uint32_t>(kSnackCount)));
}

void RoboBuddyGame::exit() {
  ctx_ = nullptr;
  reset_state();
}

uint32_t RoboBuddyGame::active_contexts() const {
  return static_cast<uint32_t>(Context::General);
}

void RoboBuddyGame::set_mood(Mood m) {
  mood_ = m;
  moods_seen_ |= 1u << static_cast<uint32_t>(m);
}

void RoboBuddyGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  idle_ms_ = 0;
  dancing_ = false;
  ctx_->feedback.play(Cue::PROMPT, kPromptPlay);
}

void RoboBuddyGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  phase_ms_ = saturating_add(phase_ms_, dt_ms);
  switch (phase_) {
    case Phase::INTRO:
      if (phase_ms_ >= kIntroMs) start_play();
      break;
    case Phase::CELEBRATE:
      if (phase_ms_ >= (dancing_ ? kDanceMs : kReactMs)) {
        phase_ = Phase::PLAY;
        phase_ms_ = 0;
        idle_ms_ = 0;
        dancing_ = false;
      }
      break;
    case Phase::PLAY:
      idle_ms_ = saturating_add(idle_ms_, dt_ms);
      if (idle_ms_ >= kCalmAfterMs && mood_ != Mood::CALM) set_mood(Mood::CALM);
      break;
    case Phase::SLEEP:
      break;  // naps until the child wakes the robot; never times out as a failure
  }
}

Action RoboBuddyGame::hit_test(int16_t x, int16_t y) const {
  if (kHead.contains(x, y)) return Action::PET;
  if (kBelly.contains(x, y)) return Action::TICKLE;
  if (kFoodButton.contains(x, y)) return Action::FEED;
  if (kSleepButton.contains(x, y)) return Action::SLEEP;
  return Action::NONE;
}

void RoboBuddyGame::show_hint() {
  hint_ms_ = kHintMs;
  ctx_->feedback.play(Cue::PROMPT, phase_ == Phase::SLEEP ? kPromptWake : kPromptPlay);
}

void RoboBuddyGame::perform(Action action) {
  switch (action) {
    case Action::PET:
    case Action::TICKLE:
    case Action::FEED:
      react(action);
      break;
    case Action::SLEEP:
      go_to_sleep();
      break;
    case Action::NONE:
    case Action::WAKE:
      break;
  }
}

void RoboBuddyGame::react(Action action) {
  uint8_t pose = static_cast<uint8_t>(ctx_->rng.below(kPoseCount));
  if (action == last_action_ && pose == pose_) pose = static_cast<uint8_t>((pose + 1) % kPoseCount);
  pose_ = pose;

  Mood m = Mood::HAPPY;
  if (action == Action::TICKLE) m = Mood::SILLY;
  if (action == Action::FEED) {
    m = Mood::YUMMY;
    // Next snack is always different from the one just eaten.
    const uint32_t step = 1u + ctx_->rng.below(static_cast<uint32_t>(kSnackCount - 1));
    snack_ = static_cast<Snack>((static_cast<uint32_t>(snack_) + step) % kSnackCount);
  }
  set_mood(m);
  last_action_ = action;
  ++reactions_;
  dancing_ = reactions_ % kDanceEvery == 0;
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  hint_ms_ = 0;
  ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(m));
  ctx_->feedback.play(Cue::NOTE, pose_);
  if (dancing_) ctx_->feedback.play(Cue::CELEBRATE, 0);
}

void RoboBuddyGame::go_to_sleep() {
  set_mood(Mood::SLEEPY);
  last_action_ = Action::SLEEP;
  ++naps_;
  phase_ = Phase::SLEEP;
  phase_ms_ = 0;
  hint_ms_ = 0;
  dancing_ = false;
  ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(Mood::SLEEPY));
  ctx_->feedback.play(Cue::AMBIENT, 0);  // one soft snore cue; Z bubbles carry the ongoing sleep visual
}

void RoboBuddyGame::wake_up() {
  // Short stretch-and-wave animation, then back to PLAY.
  set_mood(Mood::HAPPY);
  last_action_ = Action::WAKE;
  pose_ = 0;
  dancing_ = false;
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  hint_ms_ = 0;
  ctx_->feedback.play(Cue::REACTION, kWakeReaction);
}

void RoboBuddyGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // input during a reaction is ignored

  if (phase_ == Phase::INTRO) {
    start_play();  // any input skips the intro; it is not counted as an action
    return;
  }

  const bool touch = event.source == Source::TOUCH;
  const bool hello = touch && kHelloButton.contains(event.x, event.y);

  if (phase_ == Phase::SLEEP) {
    if (dozing()) return;  // let the yawn finish so a double tap doesn't instantly wake
    if (hello || (!touch && event.intent == Intent::HELP)) {
      show_hint();
      return;
    }
    bool wake = false;
    if (touch) {
      wake = hit_test(event.x, event.y) != Action::NONE;
      if (wake) ctx_->feedback.play(Cue::TAP, 0);
    } else {
      wake = event.intent == Intent::YES || event.intent == Intent::NO || event.intent == Intent::AGAIN;
    }
    if (wake) wake_up();
    return;
  }

  // PLAY
  if (touch) {
    if (hello) {
      show_hint();
      return;
    }
    const Action action = hit_test(event.x, event.y);
    if (action == Action::NONE) return;
    ctx_->feedback.play(Cue::TAP, 0);
    perform(action);
    return;
  }

  switch (event.intent) {
    case Intent::YES:
      perform(Action::PET);
      break;
    case Intent::NO:
      perform(Action::TICKLE);  // robot giggles "no no no!" with a silly wobble
      break;
    case Intent::AGAIN:
      perform(last_action_ == Action::NONE || last_action_ == Action::WAKE ? Action::PET : last_action_);
      break;
    case Intent::HELP:
      show_hint();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::robo_buddy
