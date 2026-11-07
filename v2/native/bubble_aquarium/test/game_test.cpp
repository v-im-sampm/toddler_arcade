#include "game/aquarium.hpp"
#include <cassert>
#include <cstdio>

struct Silent : aquarium::Audio {
  void play(aquarium::Cue, int) override {}
  void bedtime(bool) override {}
  void playing(bool) override {}
};
void advance(aquarium::Game& game, int steps) {
  for (int i = 0; i < steps; ++i) game.tick(0.05f);
}
int main() {
  Silent audio;
  aquarium::Game game(audio);
  assert(game.phase() == aquarium::Phase::Start);
  game.down(150, 120);
  assert(game.phase() == aquarium::Phase::Intro);
  advance(game, 31);
  assert(game.phase() == aquarium::Phase::Swim);
  aquarium::Game drag_game(audio, 7);
  drag_game.down(150, 120);
  advance(drag_game, 31);
  drag_game.down(100, 28);
  advance(drag_game, 3);
  drag_game.move(132, 28);
  drag_game.up();
  assert(drag_game.pellet_count() == 2);
  drag_game.down(220, 28);
  advance(drag_game, 12);
  assert(drag_game.bubble_count() > 0);
  const auto bubble = drag_game.bubbles()[0];
  const int before = drag_game.bubble_count();
  drag_game.down(bubble.x, bubble.y);
  assert(drag_game.bubble_count() == before - 1);
  drag_game.down(160, 28);
  assert(drag_game.bed());
  advance(drag_game, 200);
  drag_game.down(210, 140);
  assert(drag_game.fish()[0].mode == aquarium::Mode::Sleep ||
         drag_game.fish()[1].mode == aquarium::Mode::Sleep);
  for (int i = 0; i < 20; ++i) { game.down(100, 28); game.up(); advance(game, 3); }
  assert(game.pellet_count() <= 6);
  for (int i = 0; i < 20; ++i) { game.down(220, 28); game.up(); advance(game, 3); }
  assert(game.bubble_count() <= 8);
  game.down(160, 28); game.up();
  assert(game.bed());
  advance(game, 300);
  assert(game.dim() > 0.99f);
  game.down(160, 28); game.up();
  assert(!game.bed());
  game.down(100, 28); game.move(180, 30); game.up();
  assert(game.pellet_count() <= 6);
  advance(game, 2400);
  assert(game.meals() >= 3);
  assert(game.visits() >= 1);
  game.down(20, 20);
  assert(game.phase() == aquarium::Phase::Start);
  Silent other_audio, replay_audio;
  aquarium::Game first(other_audio), replay(replay_audio);
  first.down(150, 120); replay.down(150, 120);
  advance(first, 32); advance(replay, 32);
  first.down(100, 28); replay.down(100, 28);
  advance(first, 120); advance(replay, 120);
  assert(first.meals() == replay.meals());
  assert(first.fish()[0].x == replay.fish()[0].x);
  std::puts("Aquarium core deterministic; feeding/drag/popping/caps/visitor/bedtime/reset OK");
}
