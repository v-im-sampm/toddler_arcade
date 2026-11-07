#include "game/aquarium.hpp"
#include "ui/aquarium_view.hpp"
#include <SDL2/SDL.h>
#include "lvgl.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
constexpr int w = 320, h = 240;
struct Sound final : aquarium::Audio {
  std::atomic<int> note{0};
  std::atomic<bool> sleep{false};
  std::atomic<bool> active{false};
  SDL_AudioDeviceID device = 0;
  float phase = 0, pad_phase = 0, envelope = 0;
  uint64_t samples = 0;
  void play(aquarium::Cue cue, int variant) override {
    const int degrees[] = {0, 1, 2, 3, 4};
    int degree = degrees[(static_cast<int>(cue) + variant + 20) % 5];
    note.store(degree + 1, std::memory_order_relaxed);
  }
  void bedtime(bool on) override { sleep.store(on, std::memory_order_relaxed); }
  void playing(bool on) override { active.store(on, std::memory_order_relaxed); }
  static void callback(void* context, Uint8* bytes, int length) {
    auto& a = *static_cast<Sound*>(context);
    auto* out = reinterpret_cast<int16_t*>(bytes);
    constexpr double tau = 6.283185307179586;
    constexpr double notes[] = {659.26, 739.99, 830.61, 987.77, 1108.73};
    constexpr int melody[] = {0, 2, 3, 2, 0, 1, 2, 4};
    const int incoming = a.note.exchange(0, std::memory_order_relaxed);
    static int current = 0;
    if (incoming > 0) { current = incoming - 1; a.envelope = 0.11f; }
    for (int i = 0; i < length / 2; ++i) {
      const bool night = a.sleep.load(std::memory_order_relaxed);
      double seconds_per_beat = 60.0 / (night ? 60 : 70);
      int beat = static_cast<int>(a.samples / (16000 * seconds_per_beat)) % 8;
      double pad_hz = notes[melody[beat]] * 0.25;
      a.pad_phase += static_cast<float>(tau * pad_hz / 16000);
      a.phase += static_cast<float>(tau * notes[current] / 16000);
      a.envelope *= 0.9994f;
      double pad = a.active.load(std::memory_order_relaxed) ? std::sin(a.pad_phase) * (night ? 0.008 : 0.014) : 0;
      double cue = a.active.load(std::memory_order_relaxed) ? std::sin(a.phase) * a.envelope : 0;
      out[i] = static_cast<int16_t>(std::clamp(pad + cue, -0.2, 0.2) * 32767);
      ++a.samples;
    }
  }
  bool start() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want{};
    want.freq = 16000; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 256; want.callback = callback; want.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (device) SDL_PauseAudioDevice(device, 0);
    return device != 0;
  }
  ~Sound() override { if (device) SDL_CloseAudioDevice(device); }
};

struct Frame {
  std::array<uint16_t, w * h> pixels{};
  std::array<uint16_t, w * 24> buf1{}, buf2{};
};
Frame* current_frame = nullptr;
uint32_t sim_ms = 0;
std::string asset_directory() {
  char* base = SDL_GetBasePath();
  if (!base) throw std::runtime_error(std::string("Cannot locate executable: ") + SDL_GetError());
  std::string result = std::string(base) + "assets";
  SDL_free(base);
  return result;
}
uint32_t clock_ms() { return sim_ms; }
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) {
  auto* src = reinterpret_cast<uint16_t*>(data);
  const int width = area->x2 - area->x1 + 1;
  for (int y = area->y1; y <= area->y2; ++y)
    if (y >= 0 && y < h && area->x1 >= 0 && area->x2 < w)
      std::memcpy(&current_frame->pixels[y * w + area->x1], src + (y - area->y1) * width, width * 2);
  lv_display_flush_ready(display);
}
bool bmp(const char* path, const Frame& frame) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const uint32_t file_size = 54 + w * h * 3;
  uint8_t header[54]{'B', 'M'};
  std::memcpy(header + 2, &file_size, 4);
  header[10] = 54; header[14] = 40;
  uint32_t width = w, height = h;
  std::memcpy(header + 18, &width, 4); std::memcpy(header + 22, &height, 4);
  header[26] = 1; header[28] = 24;
  std::fwrite(header, 1, 54, f);
  for (int y = h - 1; y >= 0; --y) for (int x = 0; x < w; ++x) {
    uint16_t p = frame.pixels[y * w + x];
    uint8_t rgb[3] = {static_cast<uint8_t>((p & 31) * 255 / 31),
                      static_cast<uint8_t>(((p >> 5) & 63) * 255 / 63),
                      static_cast<uint8_t>((p >> 11) * 255 / 31)};
    std::fwrite(rgb, 1, 3, f);
  }
  return std::fclose(f) == 0;
}
int check(const char* screenshot, uint32_t seed) {
  Frame frame;
  current_frame = &frame;
  lv_tick_set_cb(clock_ms);
  auto* display = lv_display_create(w, h);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, frame.buf1.data(), frame.buf2.data(), sizeof(frame.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  Sound sound;
  aquarium::Game game(sound, seed);
  aquarium::View view;
  view.create(lv_screen_active(), asset_directory());
  game.down(160, 120);
  for (int i = 0; i < 50; ++i) {
    if (i == 32) game.down(100, 28);
    game.tick(0.05f); sim_ms += 50; view.sync(game); lv_timer_handler();
  }
  if (game.phase() != aquarium::Phase::Swim || game.pellet_count() < 1) return 2;
  game.down(220, 28);
  game.down(160, 28);
  for (int i = 0; i < 40; ++i) {
    game.tick(0.05f); sim_ms += 50; view.sync(game); lv_timer_handler();
  }
  if (!game.bed() || game.bubble_count() < 1) return 3;
  if (screenshot && !bmp(screenshot, frame)) { std::fprintf(stderr, "Cannot write %s\n", screenshot); return 4; }
  std::printf("host check OK: phase=swim bed=on pellets=%d bubbles=%d meals=%d screenshot=%s\n",
              game.pellet_count(), game.bubble_count(), game.meals(), screenshot ? screenshot : "(none)");
  return 0;
}
}

int main(int argc, char** argv) {
  int zoom = 2; bool run_check = false, mute = false;
  uint32_t seed = 1234; const char* screenshot = nullptr;
  for (int i = 1; i < argc; ++i) {
    const std::string option = argv[i];
    if (option == "--check") run_check = true;
    else if (option == "--mute") mute = true;
    else if ((option == "--zoom" || option == "--seed" || option == "--screenshot") && i + 1 < argc) {
      if (option == "--zoom") zoom = std::atoi(argv[++i]);
      else if (option == "--seed") seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
      else screenshot = argv[++i];
    } else { std::fprintf(stderr, "Usage: bubble_aquarium_native [--zoom 1..4] [--seed N] [--mute] [--check --screenshot PATH]\n"); return 1; }
  }
  if (zoom < 1 || zoom > 4) { std::fputs("zoom must be 1..4\n", stderr); return 1; }
  if (SDL_Init(run_check ? SDL_INIT_TIMER : SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError()); return 1;
  }
  lv_init();
  if (run_check) return check(screenshot, seed);
  auto* display = lv_sdl_window_create(w, h);
  if (!display) { std::fputs("SDL LVGL window creation failed\n", stderr); return 1; }
  lv_sdl_window_set_zoom(display, static_cast<float>(zoom));
  lv_sdl_window_set_title(display, "Bubble Aquarium (LVGL desktop)");
  lv_tick_set_cb(SDL_GetTicks);
  Sound sound;
  if (!mute && !sound.start()) std::fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
  aquarium::Game game(sound, seed);
  aquarium::View view;
  view.create(lv_screen_active(), asset_directory());
  bool pressed = false;
  uint32_t last = SDL_GetTicks();
  std::puts("Bubble Aquarium: click to play, drag fish or food jar, moon=bedtime, Home=restart; R reduced motion, B bedtime, Esc quit");
  for (;;) {
    const uint32_t now = SDL_GetTicks();
    int mx = 0, my = 0;
    const bool down = (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    const float x = static_cast<float>(mx) / zoom, y = static_cast<float>(my) / zoom;
    if (down && !pressed) game.down(x, y);
    else if (down) game.move(x, y);
    else if (pressed) game.up();
    pressed = down;
    game.tick(static_cast<float>(now - last) / 1000);
    last = now;
    view.sync(game);
    const uint32_t idle = lv_timer_handler();
    const auto* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_ESCAPE]) break;
    static bool b = false, r = false;
    if (keys[SDL_SCANCODE_B] && !b && game.phase() == aquarium::Phase::Swim) game.down(160, 28);
    if (keys[SDL_SCANCODE_R] && !r) game.set_reduced(!game.reduced());
    b = keys[SDL_SCANCODE_B] != 0; r = keys[SDL_SCANCODE_R] != 0;
    SDL_Delay(std::min<uint32_t>(idle, 5));
  }
  return 0;
}
