#include "launcher/shell.hpp"

namespace launcher {

void Shell::enter(Scene s) {
  scene_ = s;
  pressed_ = -1;
  press_t_ = 0;
  time_ = 0;
  guard_ = kGuardSeconds;
}

Event Shell::down(double x, double y) {
  if (guard_ > 0) return Event::Ignored;
  if (scene_ != Scene::Menu) {
    if (x >= kHomeX && x < kHomeX + kHomeW && y >= kHomeY && y < kHomeY + kHomeH) {
      enter(Scene::Menu);
      return Event::Home;
    }
    return Event::Forward;
  }
  if (pressed_ >= 0) return Event::Ignored;  // already launching
  pressed_ = x < kWidth / 2 ? 0 : 1;
  press_t_ = 0;
  return Event::Pressed;
}

Event Shell::tick(double dt) {
  if (dt < 0) dt = 0;
  time_ += dt;
  if (guard_ > 0) guard_ = guard_ > dt ? guard_ - dt : 0;
  if (scene_ != Scene::Menu || pressed_ < 0) return Event::None;
  press_t_ += dt;
  if (press_t_ < kPressSeconds) return Event::None;
  enter(kTileGame[pressed_]);
  return Event::Launch;
}

}  // namespace launcher
