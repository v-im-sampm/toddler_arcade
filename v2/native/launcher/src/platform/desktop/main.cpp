// Little Arcade v2 launcher, desktop build: LVGL 9 + SDL2 window, mouse = touch (including
// Aquarium drags). Audio goes through the same shared AudioFeedback (synth cues + Color Pop's
// score via GatedScore + VolumeLimiter) that the device uses, at a conservative level.
#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "launcher/arcade.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "lvgl.h"

namespace {
constexpr int W = launcher::kWidth, H = launcher::kHeight;
constexpr double kStep = 1.0 / 64;
constexpr uint8_t kDesktopLevel = 3;  // of VolumeLimiter::kMaxLevel (6): quiet by default

std::string base_path() {
  char* base = SDL_GetBasePath();
  std::string result = base ? base : "";
  if (base) SDL_free(base);
  return result;
}

std::vector<uint8_t> g_pack_bytes;
bool load_pack(color_pop::AssetPack& pack, std::string path) {
  if (path.empty()) path = base_path() + "color_pop.pack";
  std::ifstream in(path, std::ios::binary);
  if (!in) { std::fprintf(stderr, "Cannot open %s (build the launcher_native target)\n", path.c_str()); return false; }
  g_pack_bytes.assign(std::istreambuf_iterator<char>(in), {});
  std::string error;
  if (!pack.load(g_pack_bytes.data(), g_pack_bytes.size(), &error)) {
    std::fprintf(stderr, "Bad asset pack %s: %s\n", path.c_str(), error.c_str());
    return false;
  }
  return true;
}

struct DesktopAudio {
  little_arcade::audio::AudioFeedback feedback;
  SDL_AudioDeviceID device = 0;
  static void callback(void* user, Uint8* bytes, int length) {
    static_cast<DesktopAudio*>(user)->feedback.render(reinterpret_cast<int16_t*>(bytes),
                                                      static_cast<std::size_t>(length) / 2);
  }
  bool open() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want{};
    want.freq = static_cast<int>(little_arcade::audio::kSampleRateHz);
    want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 256;
    want.callback = callback; want.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!device) return false;
    feedback.set_level(kDesktopLevel);  // limiter starts muted and ramps; nothing plays until a tap
    SDL_PauseAudioDevice(device, 0);
    return true;
  }
  ~DesktopAudio() { if (device) SDL_CloseAudioDevice(device); }
};

// ---------------- offscreen check ----------------
struct Frame {
  std::array<uint16_t, W * H> pixels{};
  std::array<uint16_t, W * 24> buf1{}, buf2{};
};
Frame* g_frame = nullptr;
uint32_t g_sim_ms = 0;
uint32_t sim_clock() { return g_sim_ms; }
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) {
  const auto* src = reinterpret_cast<const uint16_t*>(data);
  const int width = area->x2 - area->x1 + 1;
  for (int y = area->y1; y <= area->y2; ++y)
    std::memcpy(&g_frame->pixels[y * W + area->x1], src + (y - area->y1) * width, width * 2);
  lv_display_flush_ready(display);
}
bool write_bmp(const std::string& path, const Frame& f) {
  FILE* out = std::fopen(path.c_str(), "wb");
  if (!out) return false;
  const uint32_t size = 54 + W * H * 3, width = W, height = H;
  uint8_t header[54]{'B', 'M'};
  std::memcpy(header + 2, &size, 4);
  header[10] = 54; header[14] = 40;
  std::memcpy(header + 18, &width, 4); std::memcpy(header + 22, &height, 4);
  header[26] = 1; header[28] = 24;
  std::fwrite(header, 1, 54, out);
  for (int y = H - 1; y >= 0; --y)
    for (int x = 0; x < W; ++x) {
      const uint16_t p = f.pixels[y * W + x];
      const uint8_t bgr[3] = {static_cast<uint8_t>((p & 31) * 255 / 31), static_cast<uint8_t>(((p >> 5) & 63) * 255 / 63),
                              static_cast<uint8_t>((p >> 11) * 255 / 31)};
      std::fwrite(bgr, 1, 3, out);
    }
  return std::fclose(out) == 0;
}

int run_check(const std::string& pack_path, const std::string& aq_dir, uint32_t seed, const std::string& shots) {
  color_pop::AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  auto frame = std::make_unique<Frame>();
  g_frame = frame.get();
  lv_tick_set_cb(sim_clock);
  lv_display_t* display = lv_display_create(W, H);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, frame->buf1.data(), frame->buf2.data(), sizeof(frame->buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);

  auto audio = std::make_unique<little_arcade::audio::AudioFeedback>();
  auto arcade = std::make_unique<launcher::Arcade>(*audio, seed);
  audio->attach_score(&arcade->score());
  audio->set_level(kDesktopLevel);
  std::string error;
  if (!arcade->create_menu(pack, aq_dir, &error) || !arcade->create_games(pack, aq_dir, &error)) {
    std::fprintf(stderr, "check failed: %s\n", error.c_str());
    return 1;
  }
  lv_mem_monitor_t mem{};
  lv_mem_monitor(&mem);
  const size_t lv_used_after_create = mem.total_size - mem.free_size;

  std::vector<int16_t> pcm(250);
  int16_t peak = 0;
  double render_max = 0;
  auto frames = [&](int n) {
    for (int i = 0; i < n; ++i) {
      arcade->frame(kStep);
      g_sim_ms += static_cast<uint32_t>(kStep * 1000 + 0.5);
      const auto t0 = std::chrono::steady_clock::now();
      lv_refr_now(display);
      render_max = std::max(render_max, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
      audio->render(pcm.data(), pcm.size());  // 16000 / 64 samples per frame
      for (int16_t s : pcm) peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(s)));
    }
  };
  auto shot = [&](const char* name) {
    if (!shots.empty() && !write_bmp(shots + "/" + name, *frame)) std::fprintf(stderr, "cannot write %s\n", name);
  };
  auto fail = [&](int code, const char* why) {
    std::fprintf(stderr, "check failed: %s (scene=%d cp_phase=%d aq_phase=%d)\n", why,
                 static_cast<int>(arcade->shell().scene()), static_cast<int>(arcade->color_pop().phase()),
                 static_cast<int>(arcade->aquarium().phase()));
    return code;
  };
  using launcher::Scene;

  frames(40);
  if (arcade->shell().scene() != Scene::Menu) return fail(2, "boot must show the menu");
  if (peak != 0) return fail(3, "menu must be silent until a tap");
  const auto menu_pixels = frame->pixels;
  shot("launcher_menu.bmp");

  // Color Pop: left half, straight into its intro, then play.
  arcade->down(80, 200); arcade->up();
  frames(4);
  if (arcade->shell().pressed() != 0) return fail(4, "left tile not pressed");
  shot("launcher_menu_pressed.bmp");
  frames(20);
  if (arcade->shell().scene() != Scene::ColorPop) return fail(5, "Color Pop did not launch");
  frames(150);
  if (arcade->color_pop().phase() != color_pop::Phase::Play) return fail(6, "Color Pop did not reach play");
  shot("launcher_color_pop.bmp");
  if (arcade->score().gain() != launcher::GatedScore::kFadeSamples) return fail(7, "Color Pop score not audible");
  // A tap just outside Home stays in the game.
  arcade->down(60, 30); arcade->up();
  frames(4);
  if (arcade->shell().scene() != Scene::ColorPop) return fail(8, "non-Home tap left the game");
  arcade->down(24, 24); arcade->up();
  frames(1);
  if (arcade->shell().scene() != Scene::Menu || arcade->color_pop().phase() != color_pop::Phase::Start)
    return fail(9, "Home did not return to the menu");
  frames(20);
  if (arcade->score().gain() != 0) return fail(10, "Color Pop waves still playing over the menu");

  // Bubble Aquarium: right half, feed with a drag of the shaker, Home.
  frames(10);
  arcade->down(250, 60); arcade->up();
  frames(30);
  if (arcade->shell().scene() != Scene::Aquarium) return fail(11, "Aquarium did not launch");
  frames(200);
  if (arcade->aquarium().phase() != aquarium::Phase::Swim) return fail(12, "Aquarium did not reach swim");
  arcade->down(100, 28);
  for (int i = 0; i < 10; ++i) { arcade->move(100 + i * 6, 28 + i); frames(1); }
  arcade->up();
  frames(20);
  if (arcade->aquarium().pellet_count() < 1) return fail(13, "Aquarium feed did not work through the launcher");
  shot("launcher_aquarium.bmp");
  arcade->down(10, 10); arcade->up();
  frames(1);
  if (arcade->shell().scene() != Scene::Menu || arcade->aquarium().phase() != aquarium::Phase::Start)
    return fail(14, "Aquarium Home did not return to the menu");

  // The menu must come back pixel-identical (same scene time, nothing left over from the games).
  frames(39);
  shot("launcher_menu_return.bmp");
  long long differ = 0;
  for (std::size_t i = 0; i < menu_pixels.size(); ++i) differ += menu_pixels[i] != frame->pixels[i];
  if (differ) { std::fprintf(stderr, "%lld px differ\n", differ); return fail(15, "menu not restored after games"); }

  // Second round trip to catch state that only breaks on re-entry.
  for (int tile = 0; tile < 2; ++tile) {
    frames(30);
    arcade->down(tile ? 300 : 20, 120); arcade->up();
    frames(60);
    if (arcade->shell().scene() != launcher::kTileGame[tile]) return fail(16, "re-entry failed");
    frames(30);
    arcade->down(5, 5); arcade->up();
    frames(1);
    if (arcade->shell().scene() != Scene::Menu) return fail(17, "re-entry Home failed");
  }
  lv_mem_monitor(&mem);
  std::printf("host check OK: seed=%u menu->Color Pop->menu->Aquarium(feed)->menu x2; audio peak %d; "
              "host render max %.2f ms; LVGL pool %zu/%zu used after create, %zu now, max %zu; screenshots=%s\n",
              seed, peak, render_max, lv_used_after_create, static_cast<size_t>(mem.total_size),
              static_cast<size_t>(mem.total_size - mem.free_size), static_cast<size_t>(mem.max_used),
              shots.empty() ? "(none)" : shots.c_str());
  return 0;
}

int run_window(const std::string& pack_path, const std::string& aq_dir, uint32_t seed, int zoom, bool mute,
               double exit_after) {
  lv_display_t* display = lv_sdl_window_create(W, H);
  if (!display) { std::fputs("SDL LVGL window creation failed\n", stderr); return 1; }
  lv_sdl_window_set_zoom(display, static_cast<float>(zoom));
  lv_sdl_window_set_title(display, "Little Arcade (LVGL desktop)");
  lv_tick_set_cb(SDL_GetTicks);
  color_pop::AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  auto audio = std::make_unique<DesktopAudio>();
  auto arcade = std::make_unique<launcher::Arcade>(audio->feedback, seed);
  audio->feedback.attach_score(&arcade->score());
  std::string error;
  if (!arcade->create_menu(pack, aq_dir, &error) || !arcade->create_games(pack, aq_dir, &error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  if (!mute && !audio->open()) std::fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
  std::puts("Little Arcade: click the left tile for Color Pop, the right tile for Bubble Aquarium; "
            "Home (top-left) returns to this menu. Esc quits.");
  bool pressed = false;
  uint32_t last = SDL_GetTicks();
  const uint32_t began = last;
  for (;;) {
    const uint32_t now = SDL_GetTicks();
    int mx = 0, my = 0;
    const bool down = (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    const double x = static_cast<double>(mx) / zoom, y = static_cast<double>(my) / zoom;
    if (down && !pressed) arcade->down(x, y);
    else if (down) arcade->move(x, y);
    else if (pressed) arcade->up();
    pressed = down;
    arcade->frame((now - last) / 1000.0);
    last = now;
    const uint32_t idle = lv_timer_handler();
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_ESCAPE]) break;
    if (exit_after > 0 && (SDL_GetTicks() - began) / 1000.0 >= exit_after) break;
    SDL_Delay(std::min<uint32_t>(idle, 5));
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  int zoom = 2;
  bool check = false, mute = false;
  uint32_t seed = 1234;
  double exit_after = 0;
  std::string pack, aq_dir, shots;
  for (int i = 1; i < argc; ++i) {
    const std::string o = argv[i];
    const bool has = i + 1 < argc;
    if (o == "--check") check = true;
    else if (o == "--mute") mute = true;
    else if (o == "--zoom" && has) zoom = std::atoi(argv[++i]);
    else if (o == "--seed" && has) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (o == "--pack" && has) pack = argv[++i];
    else if (o == "--aquarium-assets" && has) aq_dir = argv[++i];
    else if (o == "--screenshots" && has) shots = argv[++i];
    else if (o == "--exit-after" && has) exit_after = std::atof(argv[++i]);
    else {
      std::fprintf(stderr,
                   "Usage: launcher_native [--zoom 1..4] [--seed N] [--mute] [--exit-after S]\n"
                   "       launcher_native --check [--seed N] [--screenshots DIR]\n"
                   "       common: [--pack color_pop.pack] [--aquarium-assets DIR]\n");
      return 1;
    }
  }
  if (zoom < 1 || zoom > 4) { std::fputs("zoom must be 1..4\n", stderr); return 1; }
  if (SDL_Init(check ? SDL_INIT_TIMER : SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (aq_dir.empty()) aq_dir = base_path() + "assets";
  lv_init();
  if (check) return run_check(pack, aq_dir, seed, shots);
  return run_window(pack, aq_dir, seed, zoom, mute, exit_after);
}
