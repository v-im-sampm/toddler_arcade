#pragma once
// Little Arcade v2 main menu: which scene is showing and how touches move between scenes.
// Portable (no LVGL, SDL or ESP-IDF) so the rules are host-tested; device and desktop mains
// own the games and react to the returned events.
//
// Rules (toddler-forgiving, touch-complete, no text):
// - Menu: the screen is split into two halves; a tap anywhere on the left half chooses
//   Color Pop, the right half Bubble Aquarium. The tile visibly presses for kPressSeconds and
//   then the game launches straight into play (the menu tap stands in for the game's Play tap).
// - In a game: a tap in the shared Home rect (0,0,56,56, the same rect both games draw their
//   Home button in) returns to the menu instead of reaching the game. Everything else is the
//   game's.
// - After every scene change, taps are ignored for kGuardSeconds so one bouncy press cannot
//   act twice (e.g. Home then immediately a tile).
#include <cstdint>

namespace launcher {

enum class Scene : uint8_t { Menu, ColorPop, Aquarium };

enum class Event : uint8_t {
  None,     // nothing for the caller to do
  Forward,  // in a game: pass this touch to the game
  Pressed,  // menu: a tile is pressed (play the tile sound; launch follows from tick())
  Launch,   // tick(): enter scene(); start the game
  Home,     // in a game: leave it (reset the game); the menu is showing again
  Ignored,  // swallowed by the scene-change guard
};

inline constexpr int kWidth = 320, kHeight = 240;
inline constexpr int kHomeX = 0, kHomeY = 0, kHomeW = 56, kHomeH = 56;
inline constexpr double kPressSeconds = 0.18;
inline constexpr double kGuardSeconds = 0.35;

// Tile rectangles used by the menu view (the hit rule is the screen halves, not these).
struct Rect { int x, y, w, h; };
inline constexpr Rect kTiles[2] = {{14, 70, 140, 108}, {166, 70, 140, 108}};
inline constexpr Scene kTileGame[2] = {Scene::ColorPop, Scene::Aquarium};

class Shell {
 public:
  Event down(double x, double y);
  Event tick(double dt);

  Scene scene() const { return scene_; }
  int pressed() const { return pressed_; }  // menu tile being pressed, -1 when none
  double press_time() const { return press_t_; }
  double time() const { return time_; }  // seconds in the current scene (drives menu motion)

 private:
  void enter(Scene s);
  Scene scene_ = Scene::Menu;
  int pressed_ = -1;
  double press_t_ = 0, time_ = 0, guard_ = 0;
};

}  // namespace launcher
