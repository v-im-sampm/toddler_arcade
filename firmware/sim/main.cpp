// Little Arcade desktop simulator: launcher + Home + real game logic and
// LVGL views, including Color Pop, Hatch-a-Pet, Rainbow Piano and Robo Buddy.
//
// Interactive:  little_arcade_sim [--zoom N] [--seed N] [--mute] [--volume 0..6]
//   Sounds play through the PC speakers using the device synth. Like the
//   device, sound starts muted until a parent opts in: hold the Mic corner
//   (top-right) for 5 s, answer the sum, pick a level. --volume N stands in
//   for a level a parent already saved (the sim does not persist settings).
//   Mouse = touch (Home and the launcher work like the device).
//   Keys = voice intents: R B G Y colours, A AGAIN, H HELP, J YES, N NO,
//     X NEXT, Backspace BACK, C COLORS, E EGGS, M MUSIC, T ROBOT. Esc quits.
// Benchmark:    little_arcade_sim --bench [--game color_pop|hatch_a_pet|rainbow_piano|robo_buddy]
//                 [--seconds N] [--seed N] [--frame-ms N] [--shots DIR]
//   Headless, deterministic simulated clock. Starts on the launcher, taps the
//   game's tile, then a scripted player plays through the SceneManager.
//   Renders RGB565 into partial buffers like the device.
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

#include "little_arcade/arcade.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "lvgl.h"

using namespace little_arcade;

namespace {

struct Options {
  bool bench = false;
  GameId game = GameId::COLOR_POP;
  uint32_t seconds = 60;
  uint32_t seed = 0;
  uint32_t frame_ms = LV_DEF_REFR_PERIOD;
  int zoom = 3;
  bool mute = false;
  uint32_t volume = 0;  // parent-saved level stand-in; 0 = muted (default)
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

// Stands in for the audio layer: prints cues instead of playing sounds.
class ConsoleFeedback final : public IFeedback {
 public:
  bool quiet = false;
  uint32_t count = 0;
  void play(Cue cue, uint8_t variant) override {
    ++count;
    if (!quiet) std::printf("[audio] %s %u\n", cue_name(cue), static_cast<unsigned>(variant));
  }
};

// Interactive mode: prints cues and plays them through the same synth and
// volume limiter as the device, via an SDL audio callback (host speakers).
class SpeakerFeedback final : public IFeedback {
 public:
  ConsoleFeedback console;
  audio::AudioFeedback audio;
  void play(Cue cue, uint8_t variant) override {
    console.play(cue, variant);
    audio.play(cue, variant);
  }
  void set_scene(uint8_t scene) override { audio.set_scene(scene); }
};

void sdl_audio_callback(void* user, Uint8* stream, int len) {
  static_cast<audio::AudioFeedback*>(user)->render(reinterpret_cast<int16_t*>(stream),
                                                   static_cast<std::size_t>(len) / sizeof(int16_t));
}

bool open_sim_audio(audio::AudioFeedback& fb) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
  SDL_AudioSpec want{};
  want.freq = static_cast<int>(audio::kSampleRateHz);
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 256;
  want.callback = sdl_audio_callback;
  want.userdata = &fb;
  const SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
  if (dev == 0) return false;
  SDL_PauseAudioDevice(dev, 0);
  return true;
}

int16_t mid_x(const Rect& r) { return static_cast<int16_t>(r.x + r.w / 2); }
int16_t mid_y(const Rect& r) { return static_cast<int16_t>(r.y + r.h / 2); }
void tap(Arcade& a, const Rect& r, uint32_t now) { a.touch(mid_x(r), mid_y(r), now); }

class RainbowPianoPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using rainbow_piano::Phase;
    const auto& g = a.rainbow_piano();
    if (!started_) {
      started_ = true;
      next_ = now + 500;
      return;
    }
    if (now < next_) return;
    if (g.phase() == Phase::CELEBRATE) {
      next_ = now + 100;
      return;
    }
    const int key = g.follow_key();
    if (key >= 0) tap(a, rainbow_piano::RainbowPianoGame::kKeys[key], now);
    next_ = now + 420;
  }

 private:
  bool started_ = false;
  uint32_t next_ = 0;
};

class RoboBuddyPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using robo_buddy::Phase;
    const auto& g = a.robo_buddy();
    if (!started_) {
      started_ = true;
      next_ = now + robo_buddy::RoboBuddyGame::kIntroMs + 100;
      return;
    }
    if (now < next_) return;
    if (g.phase() == Phase::SLEEP) {
      if (!g.dozing()) {
        tap(a, robo_buddy::RoboBuddyGame::kHead, now);
        next_ = now + 800;
      }
      return;
    }
    if (g.phase() != Phase::PLAY) {
      next_ = now + 100;
      return;
    }
    switch (step_++ % 4) {
      case 0: tap(a, robo_buddy::RoboBuddyGame::kHead, now); break;
      case 1: tap(a, robo_buddy::RoboBuddyGame::kBelly, now); break;
      case 2: tap(a, robo_buddy::RoboBuddyGame::kFoodButton, now); break;
      default: tap(a, robo_buddy::RoboBuddyGame::kSleepButton, now); break;
    }
    next_ = now + 1500;
  }

 private:
  bool started_ = false;
  uint32_t next_ = 0;
  uint32_t step_ = 0;
};

Intent color_intent(color_pop::Color c) {
  switch (c) {
    case color_pop::Color::RED: return Intent::RED;
    case color_pop::Color::BLUE: return Intent::BLUE;
    case color_pop::Color::GREEN: return Intent::GREEN;
    case color_pop::Color::YELLOW: return Intent::YELLOW;
  }
  return Intent::UNKNOWN;
}

// Deterministic "child" for Color Pop: each round taps a wrong bubble,
// sometimes asks AGAIN via the badge, then answers by touch or (every third
// round) by voice.
class ColorPopPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using color_pop::Phase;
    const color_pop::ColorPopGame& g = a.color_pop();
    if (g.phase() != last_) {
      last_ = g.phase();
      start_ = now;
      step_ = 0;
    }
    if (last_ != Phase::PLAY) return;
    const uint32_t t = now - start_;
    const uint32_t round = g.rounds_completed();
    if (step_ == 0 && t >= 400) {
      for (std::size_t i = 0; i < g.bubble_count(); ++i) {
        if (g.bubble(i).color != g.target()) {
          tap(a, g.bubble(i).rect, now);
          break;
        }
      }
      step_ = 1;
    } else if (step_ == 1 && t >= 900) {
      if (round % 4 == 2) tap(a, color_pop::ColorPopGame::kTargetBadge, now);
      step_ = 2;
    } else if (step_ == 2 && t >= 1400) {
      if (round % 3 == 1) {
        a.voice(color_intent(g.target()), now);
      } else {
        for (std::size_t i = 0; i < g.bubble_count(); ++i) {
          if (g.bubble(i).color == g.target()) {
            tap(a, g.bubble(i).rect, now);
            break;
          }
        }
      }
      step_ = 3;
    }
  }

 private:
  color_pop::Phase last_ = color_pop::Phase::INTRO;
  uint32_t start_ = 0;
  int step_ = 0;
};

// Deterministic "child" for Hatch-a-Pet: taps the egg (one prompt replay via
// the badge on every other egg, one crack by voice YES), pets the creature
// twice, presses NO once, then YES (touch or voice) for another egg.
class HatchAPetPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using hatch_a_pet::HatchAPetGame;
    using hatch_a_pet::Phase;
    const HatchAPetGame& g = a.hatch_a_pet();
    if (g.phase() != last_) {
      last_ = g.phase();
      start_ = now;
      step_ = 0;
      next_ = 500;
    }
    const uint32_t t = now - start_;
    if (t < next_) return;
    const uint32_t n = g.hatch_count();
    if (last_ == Phase::EGG) {
      if (step_ == 0 && n % 2 == 1) {
        tap(a, HatchAPetGame::kPromptBadge, now);
        next_ = t + 900;
      } else if (step_ == 2) {
        a.voice(Intent::YES, now);
        next_ = t + 450;
      } else {
        tap(a, HatchAPetGame::kEgg, now);
        next_ = t + 450;
      }
      ++step_;
    } else if (last_ == Phase::PET) {
      switch (step_++) {
        case 0:
        case 1: tap(a, HatchAPetGame::kCreature, now); next_ = t + 800; break;
        case 2: tap(a, HatchAPetGame::kNoButton, now); next_ = t + 900; break;
        default:
          if (n % 2 == 0) {
            a.voice(Intent::YES, now);
          } else {
            tap(a, HatchAPetGame::kYesButton, now);
          }
          next_ = UINT32_MAX;
          break;
      }
    }
  }

 private:
  hatch_a_pet::Phase last_ = hatch_a_pet::Phase::INTRO;
  uint32_t start_ = 0;
  uint32_t next_ = 0;
  int step_ = 0;
};

// ---------------------------------------------------------------- benchmark
constexpr int kW = kScreenWidth;
constexpr int kH = kScreenHeight;
constexpr int kBufLines = 24;  // 1/10 screen, double-buffered, like the device

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
      line[x * 3] = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
      line[x * 3 + 1] = static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63);
      line[x * 3 + 2] = static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31);
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

// Screenshot triggers: each fires once, the first frame its condition holds.
struct ShotPlan {
  struct Shot {
    const char* name;
    bool done;
  };
  std::vector<Shot> shots;
  std::string dir, prefix;

  void maybe(std::size_t i, bool want, const uint16_t* frame) {
    if (dir.empty() || !want || shots[i].done) return;
    shots[i].done = true;
    const std::string path = dir + "/" + prefix + shots[i].name + ".bmp";
    std::printf("shot %s %s\n", path.c_str(), write_bmp(path, frame) ? "ok" : "FAILED");
  }
};

int run_bench(const Options& opt) {
  static BenchState state{};
  g_bench = &state;
  lv_tick_set_cb(sim_tick);
  lv_display_t* disp = lv_display_create(kW, kH);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, state.buf1, state.buf2, sizeof(state.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, bench_flush);

  const bool hatch = opt.game == GameId::HATCH_A_PET;
  const bool piano = opt.game == GameId::RAINBOW_PIANO;
  const bool robo = opt.game == GameId::ROBO_BUDDY;
  const uint32_t seed = opt.seed ? opt.seed : 1234;
  static ConsoleFeedback feedback;
  feedback.quiet = true;
  static Arcade arcade(feedback, seed);
  arcade.create(lv_screen_active());
  ColorPopPlayer pop_player;
  HatchAPetPlayer hatch_player;
  RainbowPianoPlayer piano_player;
  RoboBuddyPlayer robo_player;

  ShotPlan plan;
  plan.dir = opt.shots;
  plan.prefix = hatch ? "hatch_a_pet_" : (piano ? "rainbow_piano_" : (robo ? "robo_buddy_" : "color_pop_"));
  if (hatch) {
    plan.shots = {{"00_menu", false},  {"01_intro", false},     {"02_egg", false},       {"03_egg_cracked", false},
                  {"04_hint", false},  {"05_hatch", false},     {"06_hatch_pop", false}, {"07_pet_hello", false},
                  {"08_pet", false}};
  } else if (piano) {
    plan.shots = {{"00_menu", false}, {"01_intro", false}, {"02_play", false}, {"03_key_glow", false},
                  {"04_celebrate", false}};
  } else if (robo) {
    plan.shots = {{"00_menu", false}, {"01_intro", false}, {"02_calm", false}, {"03_happy", false},
                  {"04_silly", false}, {"05_yummy", false}, {"06_sleep", false}, {"07_wake", false}};
  } else {
    plan.shots = {{"00_menu", false},          {"01_intro", false},     {"02_play_two", false},
                  {"03_wrong_tap_wiggle", false}, {"04_celebrate", false}, {"05_play_four", false}};
  }

  const uint32_t launch_at = 600;  // let the launcher settle and be captured
  const Rect tile = SceneManager::kTileRects[robo ? 3 : (piano ? 2 : (hatch ? 1 : 0))];
  const uint32_t frames = opt.seconds * 1000 / opt.frame_ms;
  std::vector<double> render_us, sync_us;
  std::vector<uint64_t> pixels;
  render_us.reserve(frames);
  sync_us.reserve(frames);
  bool launched = false;
  int pop_phase = -1, hatch_phase = -1, piano_phase = -1, robo_phase = -1, last_wiggle = -1;
  uint32_t phase_start = 0, wiggle_start = 0;

  using clock = std::chrono::steady_clock;
  for (uint32_t f = 0; f < frames; ++f) {
    g_sim_now += opt.frame_ms;
    if (!launched && g_sim_now >= launch_at) {
      tap(arcade, tile, g_sim_now);
      launched = true;
    }
    const auto t0 = clock::now();
    if (arcade.layer() != Arcade::Layer::MENU) {
      if (hatch) {
        hatch_player.tick(arcade, g_sim_now);
      } else if (piano) {
        piano_player.tick(arcade, g_sim_now);
      } else if (robo) {
        robo_player.tick(arcade, g_sim_now);
      } else {
        pop_player.tick(arcade, g_sim_now);
      }
    }
    arcade.tick(g_sim_now);
    const auto t1 = clock::now();
    state.frame_pixels = 0;
    lv_timer_handler();
    const auto t2 = clock::now();
    sync_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    if (state.frame_pixels > 0) {
      render_us.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
      pixels.push_back(state.frame_pixels);
    }

    if (plan.dir.empty()) continue;
    plan.maybe(0, arcade.layer() == Arcade::Layer::MENU && g_sim_now >= launch_at - 2 * opt.frame_ms, state.frame);
    if (hatch) {
      using hatch_a_pet::Phase;
      const hatch_a_pet::HatchAPetGame& g = arcade.hatch_a_pet();
      if (arcade.layer() != Arcade::Layer::HATCH_A_PET) continue;
      if (static_cast<int>(g.phase()) != hatch_phase) {
        hatch_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 300, state.frame);
      plan.maybe(2, g.phase() == Phase::EGG && g.cracks() == 0 && in_phase >= 300, state.frame);
      plan.maybe(3, g.phase() == Phase::EGG && g.cracks() >= 2 && !g.wobbling(), state.frame);
      plan.maybe(4, g.phase() == Phase::EGG && g.hint_active() && in_phase >= 900, state.frame);
      plan.maybe(5, g.phase() == Phase::HATCH && in_phase >= 300, state.frame);
      plan.maybe(6, g.phase() == Phase::HATCH && in_phase >= 900, state.frame);
      plan.maybe(7, g.phase() == Phase::PET && in_phase >= 250, state.frame);
      plan.maybe(8, g.phase() == Phase::PET && g.pet_count() >= 2 && g.reacting() && in_phase >= 1300, state.frame);
    } else if (piano) {
      using rainbow_piano::Phase;
      const auto& g = arcade.rainbow_piano();
      if (arcade.layer() != Arcade::Layer::RAINBOW_PIANO) continue;
      if (static_cast<int>(g.phase()) != piano_phase) {
        piano_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 100, state.frame);
      plan.maybe(2, g.phase() == Phase::PLAY && in_phase >= 100, state.frame);
      bool glowing = false;
      for (std::size_t i = 0; i < rainbow_piano::kKeyCount; ++i) glowing = glowing || g.key_lit(i);
      plan.maybe(3, glowing, state.frame);
      plan.maybe(4, g.phase() == Phase::CELEBRATE && in_phase >= 100, state.frame);
    } else if (robo) {
      using robo_buddy::Phase;
      const auto& g = arcade.robo_buddy();
      if (arcade.layer() != Arcade::Layer::ROBO_BUDDY) continue;
      if (static_cast<int>(g.phase()) != robo_phase) {
        robo_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 100, state.frame);
      plan.maybe(2, g.phase() == Phase::PLAY && g.mood() == robo_buddy::Mood::CALM, state.frame);
      plan.maybe(3, g.mood() == robo_buddy::Mood::HAPPY && g.phase() == Phase::CELEBRATE, state.frame);
      plan.maybe(4, g.mood() == robo_buddy::Mood::SILLY && g.phase() == Phase::CELEBRATE, state.frame);
      plan.maybe(5, g.mood() == robo_buddy::Mood::YUMMY && g.phase() == Phase::CELEBRATE, state.frame);
      plan.maybe(6, g.sleeping() && !g.dozing(), state.frame);
      plan.maybe(7, g.last_action() == robo_buddy::Action::WAKE && g.phase() == Phase::CELEBRATE, state.frame);
    } else {
      using color_pop::Phase;
      const color_pop::ColorPopGame& g = arcade.color_pop();
      if (arcade.layer() != Arcade::Layer::COLOR_POP) continue;
      if (static_cast<int>(g.phase()) != pop_phase) {
        pop_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      if (g.wiggling() != last_wiggle) {
        last_wiggle = g.wiggling();
        wiggle_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 100, state.frame);
      plan.maybe(2, g.phase() == Phase::PLAY && g.bubble_count() == 2 && in_phase >= 100, state.frame);
      plan.maybe(3, last_wiggle >= 0 && g_sim_now - wiggle_start >= 66 && g.hint_active(), state.frame);
      plan.maybe(4, g.phase() == Phase::CELEBRATE && in_phase >= 400, state.frame);
      plan.maybe(5, g.phase() == Phase::PLAY && g.bubble_count() == 4 && in_phase >= 100, state.frame);
    }
  }

  lv_mem_monitor_t mem;
  lv_mem_monitor(&mem);
  double render_sum = 0, sync_sum = 0;
  for (double v : render_us) render_sum += v;
  for (double v : sync_us) sync_sum += v;
  uint64_t px_sum = 0, px_max = 0;
  for (uint64_t v : pixels) {
    px_sum += v;
    px_max = std::max(px_max, v);
  }
  const std::size_t rendered = render_us.size();
  const double avg_px = rendered ? static_cast<double>(px_sum) / static_cast<double>(rendered) : 0.0;
  const double render_avg = rendered ? render_sum / static_cast<double>(rendered) : 0.0;
  const char* name = hatch ? "hatch_a_pet" : (piano ? "rainbow_piano" : (robo ? "robo_buddy" : "color_pop"));
  const uint32_t progress = hatch ? arcade.hatch_a_pet().hatch_count()
                                  : (piano ? arcade.rainbow_piano().tunes_completed()
                                           : (robo ? arcade.robo_buddy().reactions()
                                                   : arcade.color_pop().rounds_completed()));
  const hatch_a_pet::HatchAPetView& hv = arcade.hatch_a_pet_view();

  std::printf("\n%s LVGL benchmark via Arcade (HOST DESKTOP, not device)\n", name);
  std::printf("  seed=%u frame_ms=%u sim_seconds=%u frames=%u rendered=%zu %s=%u cues=%u\n", seed, opt.frame_ms,
              opt.seconds, frames, rendered, hatch ? "hatches" : (piano ? "tunes" : (robo ? "reactions" : "rounds")),
              progress, feedback.count);
  std::printf("  render us/frame: avg=%.1f p50=%.1f p95=%.1f max=%.1f\n", render_avg, percentile(render_us, 0.5),
              percentile(render_us, 0.95), percentile(render_us, 1.0));
  std::printf("  logic+view sync us/frame: avg=%.2f p95=%.2f max=%.2f\n", sync_sum / static_cast<double>(frames),
              percentile(sync_us, 0.95), percentile(sync_us, 1.0));
  if (hatch) {
    std::printf("  art rasters=%u total_ms=%u max_ms=%u (lv_tick = simulated clock, so 0 here)\n", hv.rasters(),
                hv.total_raster_ms(), hv.max_raster_ms());
  }
  std::printf("  pixels/rendered frame: avg=%.0f (%.1f%% of screen) max=%llu\n", avg_px, 100.0 * avg_px / (kW * kH),
              static_cast<unsigned long long>(px_max));
  std::printf("  lvgl heap: used=%zu max_used=%zu total=%zu frag=%u%%\n", mem.total_size - mem.free_size,
              mem.max_used, mem.total_size, static_cast<unsigned>(mem.frag_pct));
  std::printf("JSON {\"engine\":\"lvgl-9.5.0\",\"platform\":\"host-desktop\",\"game\":\"%s\",\"seed\":%u,"
              "\"frame_ms\":%u,\"frames\":%u,\"rendered\":%zu,\"render_us_avg\":%.1f,\"render_us_p95\":%.1f,"
              "\"render_us_max\":%.1f,\"sync_us_max\":%.1f,\"px_avg\":%.0f,\"px_max\":%llu,\"heap_max_used\":%zu,"
              "\"progress\":%u,\"rasters\":%u}\n",
              name, seed, opt.frame_ms, frames, rendered, render_avg, percentile(render_us, 0.95),
              percentile(render_us, 1.0), percentile(sync_us, 1.0), avg_px, static_cast<unsigned long long>(px_max),
              mem.max_used, progress, hatch ? hv.rasters() : 0u);
  return progress > 0 ? 0 : 1;
}

// -------------------------------------------------------------- interactive
int run_interactive(const Options& opt) {
  lv_display_t* disp = lv_sdl_window_create(kW, kH);
  lv_sdl_window_set_zoom(disp, static_cast<float>(opt.zoom));
  lv_sdl_window_set_title(disp, "Little Arcade sim (desktop, not device)");
  lv_tick_set_cb(SDL_GetTicks);

  static SpeakerFeedback feedback;
  const bool audio_ok = !opt.mute && open_sim_audio(feedback.audio);
  if (!audio_ok) {
    std::printf("sim audio off (%s)\n", opt.mute ? "--mute" : SDL_GetError());
  }
  const uint32_t seed = opt.seed ? opt.seed : static_cast<uint32_t>(SDL_GetTicks() ^ 0xA5A5u) | 1u;
  static Arcade arcade(feedback, seed, static_cast<uint8_t>(opt.volume));
  arcade.create(lv_screen_active());
  int applied_level = -1;
  std::printf(
      "Little Arcade sim: click = touch. Keys: R/B/G/Y colours, A again, H help, J yes, N no, X next,\n"
      "  Backspace back, C colors, E eggs, M music, T robot, Esc quit. Sound is muted: hold the Mic corner 5 s for parent settings.\n");

  struct Key {
    SDL_Scancode code;
    Intent intent;
    bool down;
  } keys[] = {{SDL_SCANCODE_R, Intent::RED, false},       {SDL_SCANCODE_B, Intent::BLUE, false},
              {SDL_SCANCODE_G, Intent::GREEN, false},     {SDL_SCANCODE_Y, Intent::YELLOW, false},
              {SDL_SCANCODE_A, Intent::AGAIN, false},     {SDL_SCANCODE_H, Intent::HELP, false},
              {SDL_SCANCODE_J, Intent::YES, false},       {SDL_SCANCODE_N, Intent::NO, false},
              {SDL_SCANCODE_X, Intent::NEXT, false},      {SDL_SCANCODE_BACKSPACE, Intent::BACK, false},
              {SDL_SCANCODE_C, Intent::COLORS, false},    {SDL_SCANCODE_E, Intent::EGGS, false},
              {SDL_SCANCODE_M, Intent::MUSIC, false},     {SDL_SCANCODE_T, Intent::ROBOT, false}};
  for (;;) {
    const uint32_t now = SDL_GetTicks();
    // Every sample goes to the Arcade (parent-gate hold + press edges), like the device.
    int mx = 0, my = 0;
    const bool mouse_down = (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    arcade.pointer(mouse_down, static_cast<int16_t>(mx / opt.zoom), static_cast<int16_t>(my / opt.zoom), now);
    if (audio_ok && arcade.volume_level() != applied_level) {
      applied_level = arcade.volume_level();
      feedback.audio.set_level(static_cast<uint8_t>(applied_level));
      std::printf("[audio] volume level %d%s\n", applied_level, applied_level ? "" : " (muted)");
    }
    if (arcade.take_volume_changed()) std::printf("[settings] parent chose level %u (not persisted in the sim)\n", static_cast<unsigned>(arcade.volume_level()));
    arcade.tick(now);
    const uint32_t idle = lv_timer_handler();  // also pumps SDL events
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (ks[SDL_SCANCODE_ESCAPE]) break;
    for (Key& k : keys) {
      const bool down = ks[k.code] != 0;
      if (down && !k.down) {
        std::printf("[voice] %s\n", intent_name(k.intent));
        arcade.voice(k.intent, now);
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
    } else if (a == "--game" && i + 1 < argc) {
      const std::string g = argv[++i];
      if (g == "color_pop") {
        opt.game = GameId::COLOR_POP;
      } else if (g == "hatch_a_pet") {
        opt.game = GameId::HATCH_A_PET;
      } else if (g == "rainbow_piano") {
        opt.game = GameId::RAINBOW_PIANO;
      } else if (g == "robo_buddy") {
        opt.game = GameId::ROBO_BUDDY;
      } else {
        return false;
      }
    } else if (a == "--seconds") {
      if (!next(opt.seconds) || opt.seconds == 0) return false;
    } else if (a == "--seed") {
      if (!next(opt.seed)) return false;
    } else if (a == "--frame-ms") {
      if (!next(opt.frame_ms) || opt.frame_ms == 0) return false;
    } else if (a == "--zoom") {
      if (!next(zoom) || zoom == 0 || zoom > 8) return false;
      opt.zoom = static_cast<int>(zoom);
    } else if (a == "--volume") {
      if (!next(opt.volume) || opt.volume > 6) return false;
    } else if (a == "--mute") {
      opt.mute = true;
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
        "usage: little_arcade_sim [--zoom N] [--seed N] [--mute] [--volume 0..6]\n"
        "       little_arcade_sim --bench [--game color_pop|hatch_a_pet|rainbow_piano|robo_buddy] [--seconds N] [--seed N]\n"
        "                         [--frame-ms N] [--shots DIR]\n");
    return 2;
  }
  SDL_SetMainReady();
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  lv_init();
  return opt.bench ? run_bench(opt) : run_interactive(opt);
}
