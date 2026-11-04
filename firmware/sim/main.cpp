// Little Arcade desktop simulator: Color Pop (ColorPopGame + ColorPopView).
//
// Interactive:  little_arcade_sim [--zoom N] [--seed N]
//   Mouse = touch. Keys = voice intents: R B G Y (colours), A (AGAIN), H (HELP).
//   Home restarts the scene (SceneManager is not simulated); Esc quits.
// Benchmark:    little_arcade_sim --bench [--seconds N] [--seed N] [--frame-ms N] [--shots DIR]
//   Headless, deterministic simulated clock and scripted player. Renders RGB565
//   into partial buffers like the device and reports render cost per frame.
//
// Desktop only: numbers describe this PC, not the ESP32-S3 (AGENTS.md rule 8).
#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_view.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "lvgl.h"

using namespace little_arcade;
using namespace little_arcade::color_pop;

namespace {

struct Options {
  bool bench = false;
  uint32_t seconds = 60;
  uint32_t seed = 0;
  uint32_t frame_ms = LV_DEF_REFR_PERIOD;
  int zoom = 3;
  std::string shots;
};

const char* cue_name(Cue c) {
  switch (c) {
    case Cue::TAP: return "TAP";
    case Cue::PROMPT: return "PROMPT";
    case Cue::SUCCESS: return "SUCCESS";
    case Cue::TRY_AGAIN: return "TRY_AGAIN";
    case Cue::CELEBRATE: return "CELEBRATE";
    case Cue::NOTE: return "NOTE";
    case Cue::REACTION: return "REACTION";
    case Cue::AMBIENT: return "AMBIENT";
  }
  return "?";
}

const char* color_name(Color c) {
  switch (c) {
    case Color::RED: return "red";
    case Color::BLUE: return "blue";
    case Color::GREEN: return "green";
    case Color::YELLOW: return "yellow";
  }
  return "?";
}

Intent color_intent(Color c) {
  switch (c) {
    case Color::RED: return Intent::RED;
    case Color::BLUE: return Intent::BLUE;
    case Color::GREEN: return Intent::GREEN;
    case Color::YELLOW: return Intent::YELLOW;
  }
  return Intent::UNKNOWN;
}

// Stands in for the audio layer: prints cues instead of playing sounds.
class ConsoleFeedback final : public IFeedback {
 public:
  bool quiet = false;
  uint32_t count = 0;
  void play(Cue cue, uint8_t variant) override {
    ++count;
    if (quiet) return;
    if (cue == Cue::PROMPT) {
      std::printf("[audio] PROMPT \"%s!\"\n", color_name(static_cast<Color>(variant)));
    } else {
      std::printf("[audio] %s\n", cue_name(cue));
    }
  }
};

struct Scene {
  Rng rng{1};
  ConsoleFeedback feedback;
  GameContext ctx{rng, feedback};
  ColorPopGame game;
  ColorPopView view;
  ui::ChromeView chrome;
  uint32_t seed = 1;

  void build(lv_obj_t* screen) {
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    view.create(screen);
    chrome.create(screen);
  }
  void restart() {
    if (game.active()) game.exit();
    rng.reseed(seed);
    game.enter(ctx);
  }
};

int16_t mid_x(const Rect& r) { return static_cast<int16_t>(r.x + r.w / 2); }
int16_t mid_y(const Rect& r) { return static_cast<int16_t>(r.y + r.h / 2); }

// Deterministic "child": each round taps a wrong bubble, sometimes asks AGAIN
// via the badge, then answers by touch or (every third round) by voice.
class ScriptedPlayer {
 public:
  void tick(ColorPopGame& g, uint32_t now) {
    if (g.phase() != last_) {
      last_ = g.phase();
      play_start_ = now;
      step_ = 0;
    }
    if (last_ != Phase::PLAY) return;
    const uint32_t t = now - play_start_;
    const uint32_t round = g.rounds_completed();
    if (step_ == 0 && t >= 400) {
      for (std::size_t i = 0; i < g.bubble_count(); ++i) {
        if (g.bubble(i).color != g.target()) {
          g.handle(make_touch_event(mid_x(g.bubble(i).rect), mid_y(g.bubble(i).rect), now));
          break;
        }
      }
      step_ = 1;
    } else if (step_ == 1 && t >= 900) {
      if (round % 4 == 2) {
        g.handle(make_touch_event(mid_x(ColorPopGame::kTargetBadge), mid_y(ColorPopGame::kTargetBadge), now));
      }
      step_ = 2;
    } else if (step_ == 2 && t >= 1400) {
      if (round % 3 == 1) {
        g.handle(make_voice_event(color_intent(g.target()), now));
      } else {
        for (std::size_t i = 0; i < g.bubble_count(); ++i) {
          if (g.bubble(i).color == g.target()) {
            g.handle(make_touch_event(mid_x(g.bubble(i).rect), mid_y(g.bubble(i).rect), now));
            break;
          }
        }
      }
      step_ = 3;
    }
  }

 private:
  Phase last_ = Phase::INTRO;
  uint32_t play_start_ = 0;
  int step_ = 0;
};

// ---------------------------------------------------------------- benchmark
constexpr int kW = kScreenWidth;
constexpr int kH = kScreenHeight;
constexpr int kBufLines = 24;  // 1/10 screen, double-buffered, like the device plan

struct BenchState {
  uint16_t frame[kW * kH];
  alignas(64) uint8_t buf1[kW * kBufLines * 2];
  alignas(64) uint8_t buf2[kW * kBufLines * 2];
  uint64_t frame_pixels = 0;
  uint32_t flushes = 0;
};
BenchState* g_bench = nullptr;
uint32_t g_sim_now = 0;

uint32_t sim_tick() { return g_sim_now; }

void bench_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  const int w = area->x2 - area->x1 + 1;
  const auto* src = reinterpret_cast<const uint16_t*>(px_map);
  for (int y = area->y1; y <= area->y2; ++y) {
    std::memcpy(&g_bench->frame[y * kW + area->x1], src, static_cast<size_t>(w) * 2);
    src += w;
  }
  g_bench->frame_pixels += static_cast<uint64_t>(w) * static_cast<uint64_t>(area->y2 - area->y1 + 1);
  ++g_bench->flushes;
  lv_display_flush_ready(disp);
}

bool write_bmp(const std::string& path, const uint16_t* rgb565) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const uint32_t row = kW * 3, size = 54 + row * kH;
  uint8_t h[54] = {'B', 'M'};
  auto put32 = [&h](int off, uint32_t v) {
    for (int i = 0; i < 4; ++i) h[off + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  put32(2, size);
  put32(10, 54);
  put32(14, 40);
  put32(18, kW);
  put32(22, kH);
  h[26] = 1;
  h[28] = 24;
  put32(34, row * kH);
  std::fwrite(h, 1, sizeof(h), f);
  std::vector<uint8_t> line(row);
  for (int y = kH - 1; y >= 0; --y) {
    for (int x = 0; x < kW; ++x) {
      const uint16_t p = rgb565[y * kW + x];
      const uint8_t r = static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31);
      const uint8_t g = static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63);
      const uint8_t b = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
      line[x * 3] = b;
      line[x * 3 + 1] = g;
      line[x * 3 + 2] = r;
    }
    std::fwrite(line.data(), 1, row, f);
  }
  std::fclose(f);
  return true;
}

double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const std::size_t i = static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5);
  return v[std::min(i, v.size() - 1)];
}

int run_bench(const Options& opt) {
  static BenchState state{};
  g_bench = &state;
  lv_tick_set_cb(sim_tick);
  lv_display_t* disp = lv_display_create(kW, kH);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, state.buf1, state.buf2, sizeof(state.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, bench_flush);

  static Scene scene;
  scene.seed = opt.seed ? opt.seed : 1234;
  scene.feedback.quiet = true;
  scene.build(lv_screen_active());
  scene.restart();
  ScriptedPlayer player;

  const uint32_t frames = opt.seconds * 1000 / opt.frame_ms;
  std::vector<double> render_us, sync_us;
  std::vector<uint64_t> pixels;
  render_us.reserve(frames);
  struct Shot {
    const char* name;
    bool done;
  } shots[] = {{"01_intro", false}, {"02_play_two", false}, {"03_wrong_tap_wiggle", false},
               {"04_celebrate", false}, {"05_play_four", false}};
  Phase last_phase = Phase::INTRO;
  uint32_t phase_start = 0, wiggle_start = 0;
  int last_wiggle = -1;

  using clock = std::chrono::steady_clock;
  for (uint32_t f = 0; f < frames; ++f) {
    g_sim_now += opt.frame_ms;
    const auto t0 = clock::now();
    scene.game.update(opt.frame_ms);
    player.tick(scene.game, g_sim_now);
    scene.view.sync(scene.game, g_sim_now);
    const auto t1 = clock::now();
    state.frame_pixels = 0;
    lv_timer_handler();
    const auto t2 = clock::now();
    sync_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    if (state.frame_pixels > 0) {
      render_us.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
      pixels.push_back(state.frame_pixels);
    }

    if (opt.shots.empty()) continue;
    const ColorPopGame& g = scene.game;
    if (g.phase() != last_phase) {
      last_phase = g.phase();
      phase_start = g_sim_now;
    }
    if (g.wiggling() != last_wiggle) {
      last_wiggle = g.wiggling();
      wiggle_start = g_sim_now;
    }
    const uint32_t in_phase = g_sim_now - phase_start;
    const bool want[] = {
        g.phase() == Phase::INTRO && f >= 1,
        g.phase() == Phase::PLAY && g.bubble_count() == 2 && in_phase >= 100,
        last_wiggle >= 0 && g_sim_now - wiggle_start >= 66 && g.hint_active(),
        g.phase() == Phase::CELEBRATE && in_phase >= 400,
        g.phase() == Phase::PLAY && g.bubble_count() == 4 && in_phase >= 100,
    };
    for (std::size_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i) {
      if (want[i] && !shots[i].done) {
        shots[i].done = true;
        const std::string path = opt.shots + "/color_pop_" + shots[i].name + ".bmp";
        std::printf("shot %s %s\n", path.c_str(), write_bmp(path, state.frame) ? "ok" : "FAILED");
      }
    }
  }

  lv_mem_monitor_t mem;
  lv_mem_monitor(&mem);
  double render_sum = 0;
  for (double v : render_us) render_sum += v;
  uint64_t px_sum = 0, px_max = 0;
  for (uint64_t v : pixels) {
    px_sum += v;
    px_max = std::max(px_max, v);
  }
  double sync_sum = 0;
  for (double v : sync_us) sync_sum += v;
  const std::size_t rendered = render_us.size();
  const double avg_px = rendered ? static_cast<double>(px_sum) / static_cast<double>(rendered) : 0.0;

  std::printf("\nColor Pop LVGL benchmark (HOST DESKTOP, not device)\n");
  std::printf("  seed=%u frame_ms=%u sim_seconds=%u frames=%u rendered=%zu rounds=%u cues=%u\n", scene.seed,
              opt.frame_ms, opt.seconds, frames, rendered, scene.game.rounds_completed(), scene.feedback.count);
  std::printf("  render us/frame: avg=%.1f p50=%.1f p95=%.1f max=%.1f\n",
              rendered ? render_sum / static_cast<double>(rendered) : 0.0, percentile(render_us, 0.5),
              percentile(render_us, 0.95), percentile(render_us, 1.0));
  std::printf("  logic+view sync us/frame: avg=%.2f max=%.2f\n", sync_sum / static_cast<double>(frames),
              percentile(sync_us, 1.0));
  std::printf("  pixels/rendered frame: avg=%.0f (%.1f%% of screen) max=%llu\n", avg_px,
              100.0 * avg_px / (kW * kH), static_cast<unsigned long long>(px_max));
  std::printf("  lvgl heap: used=%zu max_used=%zu total=%zu frag=%u%%\n", mem.total_size - mem.free_size,
              mem.max_used, mem.total_size, static_cast<unsigned>(mem.frag_pct));
  std::printf("JSON {\"engine\":\"lvgl-9.5.0\",\"platform\":\"host-desktop\",\"seed\":%u,\"frame_ms\":%u,"
              "\"frames\":%u,\"rendered\":%zu,\"render_us_avg\":%.1f,\"render_us_p95\":%.1f,"
              "\"render_us_max\":%.1f,\"px_avg\":%.0f,\"px_max\":%llu,\"heap_max_used\":%zu,\"rounds\":%u}\n",
              scene.seed, opt.frame_ms, frames, rendered, rendered ? render_sum / static_cast<double>(rendered) : 0.0,
              percentile(render_us, 0.95), percentile(render_us, 1.0), avg_px,
              static_cast<unsigned long long>(px_max), mem.max_used, scene.game.rounds_completed());
  return scene.game.rounds_completed() > 0 ? 0 : 1;
}

// -------------------------------------------------------------- interactive
Scene* g_scene = nullptr;

void on_press(lv_event_t* e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  const auto x = static_cast<int16_t>(p.x), y = static_cast<int16_t>(p.y);
  const uint32_t now = lv_tick_get();
  if (kHomeButton.contains(x, y)) {
    std::printf("[nav] Home -> restart scene (SceneManager not simulated)\n");
    g_scene->restart();
  } else if (kMicButton.contains(x, y)) {
    std::printf("[nav] Mic: push-to-talk is simulated with keys R/B/G/Y/A/H\n");
  } else {
    g_scene->game.handle(make_touch_event(x, y, now));
  }
  (void)e;
}

int run_interactive(const Options& opt) {
  lv_display_t* disp = lv_sdl_window_create(kW, kH);
  lv_sdl_window_set_zoom(disp, static_cast<float>(opt.zoom));
  lv_sdl_window_set_title(disp, "Little Arcade sim - Color Pop (desktop, not device)");
  lv_sdl_mouse_create();
  lv_tick_set_cb(SDL_GetTicks);

  static Scene scene;
  g_scene = &scene;
  scene.seed = opt.seed ? opt.seed : static_cast<uint32_t>(SDL_GetTicks() ^ 0xA5A5u) | 1u;
  scene.build(lv_screen_active());
  lv_obj_add_event_cb(lv_screen_active(), on_press, LV_EVENT_PRESSED, nullptr);
  scene.restart();
  std::printf("Color Pop sim: click bubbles; keys R/B/G/Y = say colour, A = again, H = help, Esc = quit\n");

  struct Key {
    SDL_Scancode code;
    Intent intent;
    bool down;
  } keys[] = {{SDL_SCANCODE_R, Intent::RED, false},    {SDL_SCANCODE_B, Intent::BLUE, false},
              {SDL_SCANCODE_G, Intent::GREEN, false},  {SDL_SCANCODE_Y, Intent::YELLOW, false},
              {SDL_SCANCODE_A, Intent::AGAIN, false},  {SDL_SCANCODE_H, Intent::HELP, false}};
  uint32_t last = SDL_GetTicks();
  for (;;) {
    const uint32_t now = SDL_GetTicks();
    scene.game.update(now - last);
    last = now;
    scene.view.sync(scene.game, now);
    const uint32_t idle = lv_timer_handler();
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (ks[SDL_SCANCODE_ESCAPE]) break;
    for (Key& k : keys) {
      const bool down = ks[k.code] != 0;
      if (down && !k.down) {
        std::printf("[voice] intent %d\n", static_cast<int>(k.intent));
        scene.game.handle(make_voice_event(k.intent, now));
      }
      k.down = down;
    }
    SDL_Delay(std::min<uint32_t>(idle, 5));
  }
  return 0;
}

bool parse(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](uint32_t& out) {
      if (i + 1 >= argc) return false;
      out = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
      return true;
    };
    uint32_t zoom = 0;
    if (a == "--bench") {
      opt.bench = true;
    } else if (a == "--seconds") {
      if (!next(opt.seconds) || opt.seconds == 0) return false;
    } else if (a == "--seed") {
      if (!next(opt.seed)) return false;
    } else if (a == "--frame-ms") {
      if (!next(opt.frame_ms) || opt.frame_ms == 0) return false;
    } else if (a == "--zoom") {
      if (!next(zoom) || zoom == 0 || zoom > 8) return false;
      opt.zoom = static_cast<int>(zoom);
    } else if (a == "--shots" && i + 1 < argc) {
      opt.shots = argv[++i];
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!parse(argc, argv, opt)) {
    std::printf(
        "usage: little_arcade_sim [--zoom N] [--seed N]\n"
        "       little_arcade_sim --bench [--seconds N] [--seed N] [--frame-ms N] [--shots DIR]\n");
    return 2;
  }
  SDL_SetMainReady();
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  lv_init();
  return opt.bench ? run_bench(opt) : run_interactive(opt);
}
