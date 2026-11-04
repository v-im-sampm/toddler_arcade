#include "little_arcade/games/silly_faces.hpp"

namespace little_arcade::silly_faces {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

Emotion next_emotion(Emotion e) {
  return static_cast<Emotion>((static_cast<uint8_t>(e) + 1) % kEmotionCount);
}
}  // namespace

void SillyFacesGame::enter(GameContext& context) {
  ctx_ = &context;
  made_mask_ = 0;
  faces_made_ = 0;
  bounce_ms_ = 0;
  hint_ms_ = 0;
  idle_ms_ = 0;
  bounce_part_ = Part::NONE;
  showing_silly_ = false;
  body_color_ = static_cast<uint8_t>(ctx_->rng.below(kBodyColorCount));
  topper_ = static_cast<Topper>(ctx_->rng.below(kTopperCount));
  randomize_mixed_face();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void SillyFacesGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
  bounce_ms_ = 0;
  hint_ms_ = 0;
  idle_ms_ = 0;
  bounce_part_ = Part::NONE;
  showing_silly_ = false;
}

uint32_t SillyFacesGame::active_contexts() const {
  return static_cast<uint32_t>(Context::General);
}

void SillyFacesGame::randomize_mixed_face() {
  // Always a mixed ("silly") face that differs from the current one, so the
  // child has something to fix by tapping eyes/mouth.
  const Emotion eyes = static_cast<Emotion>(ctx_->rng.below(kEmotionCount));
  uint8_t mouth = static_cast<uint8_t>((static_cast<uint8_t>(eyes) + 1 + ctx_->rng.below(kEmotionCount - 1)) %
                                       kEmotionCount);
  if (eyes == face_.eyes && static_cast<Emotion>(mouth) == face_.mouth) {
    mouth = static_cast<uint8_t>((mouth + 1) % kEmotionCount);
  }
  if (static_cast<Emotion>(mouth) == eyes) mouth = static_cast<uint8_t>((mouth + 1) % kEmotionCount);
  face_ = Face{eyes, static_cast<Emotion>(mouth)};
}

void SillyFacesGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  idle_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, 0);
  hint_ms_ = kHintMs;  // visual twin of the spoken prompt: eyes + mouth glow
}

void SillyFacesGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  bounce_ms_ = saturating_sub(bounce_ms_, dt_ms);
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  phase_ms_ += dt_ms;
  switch (phase_) {
    case Phase::INTRO:
      if (phase_ms_ >= kIntroMs) start_play();
      break;
    case Phase::CELEBRATE:
      if (phase_ms_ >= kCelebrateMs) {
        phase_ = Phase::PLAY;
        phase_ms_ = 0;
        idle_ms_ = 0;
        showing_silly_ = false;
      }
      break;
    case Phase::PLAY:
      idle_ms_ += dt_ms;
      if (idle_ms_ >= kIdleHintMs) {
        hint_ms_ = kHintMs;  // visual only; no audio nagging, nothing is lost
        idle_ms_ = 0;
      }
      break;
  }
}

void SillyFacesGame::bounce(Part part) {
  bounce_part_ = part;
  bounce_ms_ = kBounceMs;
}

void SillyFacesGame::celebrate(bool silly, Emotion emotion) {
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  hint_ms_ = 0;
  showing_silly_ = silly;
  showing_ = emotion;
  if (silly) {
    ctx_->feedback.play(Cue::REACTION, kSillyReaction);
    return;
  }
  ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(emotion));
  ++faces_made_;
  const uint8_t before = made_mask_;
  made_mask_ = static_cast<uint8_t>(made_mask_ | (1u << static_cast<uint8_t>(emotion)));
  const bool first_full_set = made_mask_ == kAllEmotionsMask && before != kAllEmotionsMask;
  ctx_->feedback.play(Cue::CELEBRATE, first_full_set ? kCelebrateAllFour : kCelebrateEmotion);
}

void SillyFacesGame::after_part_change() {
  if (face_.matched()) celebrate(false, face_.eyes);
}

void SillyFacesGame::cycle_eyes() {
  face_.eyes = next_emotion(face_.eyes);
  bounce(Part::EYES);
  ctx_->feedback.play(Cue::TAP, kTapEyes);
  after_part_change();
}

void SillyFacesGame::cycle_mouth() {
  face_.mouth = next_emotion(face_.mouth);
  bounce(Part::MOUTH);
  ctx_->feedback.play(Cue::TAP, kTapMouth);
  after_part_change();
}

void SillyFacesGame::show_face() {
  // "Yes, show me!" Any face gets acted out: a matched one as its feeling,
  // a mixed one as a giggle. Never judged.
  bounce(Part::WHOLE);
  celebrate(!face_.matched(), face_.eyes);
}

void SillyFacesGame::mix_face() {
  randomize_mixed_face();
  bounce(Part::WHOLE);
  ctx_->feedback.play(Cue::TAP, kTapMix);
}

void SillyFacesGame::new_monster() {
  body_color_ = static_cast<uint8_t>((body_color_ + 1 + ctx_->rng.below(kBodyColorCount - 1)) %
                                     kBodyColorCount);
  topper_ = static_cast<Topper>(ctx_->rng.below(kTopperCount));
  randomize_mixed_face();
  bounce(Part::WHOLE);
  ctx_->feedback.play(Cue::TAP, kTapNewMonster);
}

void SillyFacesGame::replay_prompt() {
  ctx_->feedback.play(Cue::PROMPT, 0);
  hint_ms_ = kHintMs;
}

void SillyFacesGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // input during the act-out is ignored

  if (phase_ == Phase::INTRO) {
    start_play();  // any input skips the intro; it does not change the face
    return;
  }
  idle_ms_ = 0;

  if (event.source == Source::TOUCH) {
    if (kEyesZone.contains(event.x, event.y)) {
      cycle_eyes();
    } else if (kMouthZone.contains(event.x, event.y)) {
      cycle_mouth();
    } else if (kShowButton.contains(event.x, event.y)) {
      show_face();
    } else if (kMixButton.contains(event.x, event.y)) {
      mix_face();
    } else if (kNewMonsterButton.contains(event.x, event.y)) {
      new_monster();
    } else if (kPromptBadge.contains(event.x, event.y)) {
      replay_prompt();
    }
    return;
  }

  switch (event.intent) {
    case Intent::YES:
      show_face();
      break;
    case Intent::NO:
      mix_face();
      break;
    case Intent::NEXT:
      new_monster();
      break;
    case Intent::AGAIN:
    case Intent::HELP:
      replay_prompt();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::silly_faces
