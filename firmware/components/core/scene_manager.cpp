#include "little_arcade/scene_manager.hpp"

namespace little_arcade {

SceneManager::SceneManager(const GameEntry* entries, std::size_t count, IFeedback& feedback)
    : SceneManager(entries, count, feedback, Config{}) {}

SceneManager::SceneManager(const GameEntry* entries, std::size_t count, IFeedback& feedback,
                           Config config)
    : entries_(entries), count_(entries ? count : 0), config_(config), rng_(config.base_seed),
      context_(rng_, feedback) {}

const GameEntry* SceneManager::tile(std::size_t slot) const {
  const std::size_t index = page_ * kTilesPerPage + slot;
  return (slot < kTilesPerPage && index < count_) ? &entries_[index] : nullptr;
}

uint32_t SceneManager::active_contexts() const {
  switch (mode_) {
    case Mode::MENU:
      return Context::Menu | Context::Global;
    case Mode::PLAYING:
    case Mode::PAUSED:
      return current_->game->active_contexts() | static_cast<uint32_t>(Context::Global);
    case Mode::SLEEPING:
      return 0;
  }
  return 0;
}

bool SceneManager::accept_touch(const InputEvent& event) {
  if (have_touch_ && static_cast<uint32_t>(event.timestamp_ms - last_touch_ms_) < config_.touch_debounce_ms) {
    return false;
  }
  have_touch_ = true;
  last_touch_ms_ = event.timestamp_ms;
  return true;
}

void SceneManager::handle(const InputEvent& event) {
  const bool touch = event.source == Source::TOUCH;
  if (touch && !accept_touch(event)) return;

  if (mode_ == Mode::SLEEPING) {
    if (touch) wake();  // the waking tap is consumed
    return;
  }
  idle_ms_ = 0;

  if (!touch && !intent_allowed(event.intent, active_contexts())) return;
  if (touch && kMicButton.contains(event.x, event.y)) return;

  if (mode_ == Mode::MENU) {
    handle_menu(event);
    return;
  }

  const bool home_tap = touch && kHomeButton.contains(event.x, event.y);
  if (home_tap || event.intent == Intent::BACK) {
    go_home();
    return;
  }

  if (mode_ == Mode::PAUSED) {
    if (touch || event.intent == Intent::RESUME) {
      mode_ = Mode::PLAYING;
      current_->game->on_resume();
    }
    return;
  }

  if (event.intent == Intent::PAUSE) {
    mode_ = Mode::PAUSED;
    current_->game->on_pause();
    return;
  }
  if (event.intent == Intent::RESUME) return;

  current_->game->handle(event);
}

void SceneManager::handle_menu(const InputEvent& event) {
  if (event.source == Source::TOUCH) {
    if (kNextPageButton.contains(event.x, event.y)) {
      if (page_count()) page_ = (page_ + 1) % page_count();
      return;
    }
    for (std::size_t slot = 0; slot < kTilesPerPage; ++slot) {
      const GameEntry* entry = tile(slot);
      if (entry && kTileRects[slot].contains(event.x, event.y)) {
        launch(*entry);
        return;
      }
    }
    return;
  }

  switch (event.intent) {
    case Intent::NEXT:
      if (page_count()) page_ = (page_ + 1) % page_count();
      return;
    case Intent::START:
      if (const GameEntry* entry = tile(0)) launch(*entry);
      return;
    default:
      break;
  }
  for (std::size_t i = 0; i < count_; ++i) {
    if (entries_[i].menu_intent == event.intent) {
      launch(entries_[i]);
      return;
    }
  }
}

void SceneManager::launch(const GameEntry& entry) {
  current_ = &entry;
  rng_.reseed(config_.base_seed + launches_++);
  mode_ = Mode::PLAYING;
  context_.feedback.set_scene(static_cast<uint8_t>(entry.id));
  entry.game->enter(context_);
}

void SceneManager::go_home() {
  if (current_) current_->game->exit();
  current_ = nullptr;
  mode_ = Mode::MENU;
  context_.feedback.set_scene(IFeedback::kSceneMenu);
}

void SceneManager::sleep() {
  mode_before_sleep_ = mode_;
  if (mode_ == Mode::PLAYING) current_->game->on_pause();
  mode_ = Mode::SLEEPING;
}

void SceneManager::wake() {
  idle_ms_ = 0;
  mode_ = mode_before_sleep_;
  if (mode_ == Mode::PLAYING) current_->game->on_resume();
}

void SceneManager::update(uint32_t dt_ms) {
  if (mode_ == Mode::SLEEPING) return;
  idle_ms_ += dt_ms;
  if (idle_ms_ >= config_.sleep_after_ms) {
    sleep();
    return;
  }
  if (mode_ == Mode::PLAYING) current_->game->update(dt_ms);
}

}  // namespace little_arcade
