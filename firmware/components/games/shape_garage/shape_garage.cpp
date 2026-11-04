#include "little_arcade/games/shape_garage.hpp"

namespace little_arcade::shape_garage {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

void ShapeGarageGame::enter(GameContext& context) {
  ctx_ = &context;
  rounds_ = 0;
  new_round();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void ShapeGarageGame::exit() {
  ctx_ = nullptr;
  selected_ = -1;
  nudges_ = 0;
  hint_ms_ = 0;
  hint_slot_ = -1;
  hint_piece_ = -1;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
  snap_ms_ = 0;
  snap_slot_ = -1;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

uint32_t ShapeGarageGame::active_contexts() const {
  return static_cast<uint32_t>(Context::General);  // YES / NO; AGAIN/HELP arrive via Global
}

std::size_t ShapeGarageGame::filled_count() const {
  std::size_t n = 0;
  for (const Slot& s : slots_) n += s.filled ? 1u : 0u;
  return n;
}

void ShapeGarageGame::new_round() {
  Shape slot_shapes[kShapeCount] = {Shape::CIRCLE, Shape::SQUARE, Shape::TRIANGLE};
  Shape tray_shapes[kShapeCount] = {Shape::CIRCLE, Shape::SQUARE, Shape::TRIANGLE};
  ctx_->rng.shuffle(slot_shapes);
  ctx_->rng.shuffle(tray_shapes);
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    slots_[i] = Slot{slot_shapes[i], kSlotRects[i], false};
    pieces_[i] = Piece{tray_shapes[i], kTrayRects[i], false};
  }
  robot_style_ = static_cast<uint8_t>(rounds_ % kRobotStyles);
  selected_ = -1;
  nudges_ = 0;
  hint_ms_ = 0;
  hint_slot_ = -1;
  hint_piece_ = -1;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
  snap_ms_ = 0;
  snap_slot_ = -1;
}

void ShapeGarageGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, kPromptFixRobot);
}

void ShapeGarageGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  wiggle_ms_ = saturating_sub(wiggle_ms_, dt_ms);
  snap_ms_ = saturating_sub(snap_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play();
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    new_round();
    start_play();
  }
}

int ShapeGarageGame::slot_for(Shape s) const {
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (slots_[i].shape == s) return static_cast<int>(i);
  }
  return -1;
}

int ShapeGarageGame::piece_for(Shape s) const {
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (pieces_[i].shape == s) return static_cast<int>(i);
  }
  return -1;
}

int ShapeGarageGame::first_unplaced() const {
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (!pieces_[i].placed) return static_cast<int>(i);
  }
  return -1;
}

int ShapeGarageGame::hit_piece(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (!pieces_[i].placed && pieces_[i].rect.contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

int ShapeGarageGame::hit_slot(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (slot_hit_area(slots_[i].rect).contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

void ShapeGarageGame::set_hint(int slot_index, int piece_index) {
  hint_slot_ = slot_index;
  hint_piece_ = piece_index;
  hint_ms_ = kHintMs;
}

void ShapeGarageGame::select_piece(int index) {
  if (index < 0) return;
  selected_ = index;
  nudges_ = 0;
  ctx_->feedback.play(Cue::TAP, 0);
}

void ShapeGarageGame::put_back() {
  if (selected_ < 0) return;
  selected_ = -1;
  nudges_ = 0;
  ctx_->feedback.play(Cue::TAP, 0);
}

void ShapeGarageGame::place(int piece_index, int slot_index) {
  Piece& p = pieces_[piece_index];
  Slot& s = slots_[slot_index];
  p.placed = true;
  s.filled = true;
  selected_ = -1;
  nudges_ = 0;
  hint_ms_ = 0;
  wiggle_ms_ = 0;
  snap_slot_ = slot_index;
  snap_ms_ = kSnapMs;
  ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(p.shape));
  if (filled_count() == kPieceCount) {
    ++rounds_;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    ctx_->feedback.play(Cue::CELEBRATE, 0);
  }
}

void ShapeGarageGame::try_place(int slot_index) {
  const Piece& p = pieces_[selected_];
  const int home = slot_for(p.shape);
  if (slot_index == home) {
    place(selected_, slot_index);
    return;
  }
  if (nudges_ >= kNudgesBeforeAssist) {
    // Still trying: the piece helps itself into the right slot.
    place(selected_, home);
    return;
  }
  ++nudges_;
  wiggle_index_ = selected_;
  wiggle_ms_ = kWiggleMs;
  set_hint(home, selected_);
  ctx_->feedback.play(Cue::TRY_AGAIN, 0);
  ctx_->feedback.play(Cue::PROMPT, prompt_for(p.shape));
}

void ShapeGarageGame::replay_prompt() {
  if (selected_ >= 0) {
    const Shape s = pieces_[selected_].shape;
    set_hint(slot_for(s), selected_);
    ctx_->feedback.play(Cue::PROMPT, prompt_for(s));
    return;
  }
  const int next = first_unplaced();
  if (next >= 0) set_hint(slot_for(pieces_[next].shape), next);
  ctx_->feedback.play(Cue::PROMPT, kPromptFixRobot);
}

void ShapeGarageGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // input during the robot dance is ignored

  if (phase_ == Phase::INTRO) {
    // Any input skips the intro; it is not counted as a move.
    start_play();
    return;
  }

  if (event.source == Source::TOUCH) {
    const int piece = hit_piece(event.x, event.y);
    if (piece >= 0) {
      if (piece == selected_) {
        put_back();  // touch alternative to NO
      } else {
        select_piece(piece);
      }
      return;
    }
    const int slot = hit_slot(event.x, event.y);
    if (slot >= 0) {
      if (slots_[slot].filled) {
        ctx_->feedback.play(Cue::TAP, 0);  // already fixed; nothing changes
      } else if (selected_ >= 0) {
        try_place(slot);
      } else {
        // Slot first: lift the matching piece and show where it lives.
        const int match = piece_for(slots_[slot].shape);
        select_piece(match);
        set_hint(slot, match);
      }
      return;
    }
    if (kRobotHead.contains(event.x, event.y)) replay_prompt();  // touch alternative to AGAIN
    return;
  }

  switch (event.intent) {
    case Intent::YES:
      // Same result as tapping a piece, then its slot.
      if (selected_ >= 0) {
        place(selected_, slot_for(pieces_[selected_].shape));
      } else {
        select_piece(first_unplaced());
      }
      break;
    case Intent::NO:
      put_back();
      break;
    case Intent::AGAIN:
    case Intent::HELP:
      replay_prompt();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::shape_garage
