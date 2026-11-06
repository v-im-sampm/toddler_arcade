// Little Arcade desktop simulator: launcher + Home + real game logic and
// LVGL views, including Feed the Monster and the other games.
//
// Interactive:  little_arcade_sim [--zoom N] [--seed N] [--mute] [--no-mic] [--volume 0..6]
//   Sounds play through the PC speakers using the device synth. Like the
//   device, sound starts muted until a parent opts in: hold the Mic corner
//   (top-right) for 5 s, answer the sum, pick a level. --volume N stands in
//   for a level a parent already saved (the sim does not persist settings).
//   Sound Jar records from the PC microphone into RAM only (--no-mic: built-in
//   sounds instead).
//   Mouse = touch (Home and the launcher work like the device).
//   Keys = voice intents: R B G Y colours, A AGAIN, H HELP, J YES, N NO,
//     X NEXT, Backspace BACK, C COLORS, E EGGS, K COUNTING, M MUSIC, T ROBOT. Esc quits.
// Benchmark:    little_arcade_sim --bench [--game animal_sounds|beat_sharks|color_pop|feed_monster|hatch_a_pet|rainbow_piano|robo_buddy|sound_jar]
//                 [--seconds N] [--seed N] [--frame-ms N] [--shots DIR]
//   Headless, deterministic simulated clock. Starts on the launcher, taps the
//   game's tile, then a scripted player plays through the SceneManager.
//   Renders RGB565 into partial buffers like the device.
//
// Desktop only: numbers describe this PC, not the ESP32-S3 (AGENTS.md rule 8).
#include <SDL2/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "little_arcade/arcade.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/sampler.hpp"
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
  bool mic = true;      // Sound Jar uses the PC microphone (--no-mic turns it off)
  uint32_t volume = 0;  // parent-saved level stand-in; 0 = muted (default)
  std::string shots;
  bool snap = false;
  std::string game_name = "color_pop";
  std::string taps;    // snap: "x,y@ms;x,y@ms" relative to the game launch
  std::string at;      // snap: "ms,ms" extra screenshot times
  bool reenter = false;  // snap: go Home and relaunch, then screenshot again
};

struct GameName {
  const char* name;
  GameId id;
};
constexpr GameName kGameNames[] = {
    {"color_pop", GameId::COLOR_POP},       {"hatch_a_pet", GameId::HATCH_A_PET},
    {"rainbow_piano", GameId::RAINBOW_PIANO}, {"robo_buddy", GameId::ROBO_BUDDY},
    {"shape_garage", GameId::SHAPE_GARAGE}, {"animal_sounds", GameId::ANIMAL_SOUNDS},
    {"rocket_rescue", GameId::ROCKET_RESCUE}, {"silly_faces", GameId::SILLY_FACES},
    {"feed_monster", GameId::FEED_MONSTER}, {"bubble_aquarium", GameId::BUBBLE_AQUARIUM},
    {"sound_jar", GameId::SOUND_JAR},       {"beat_sharks", GameId::BEAT_SHARKS},
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

// Sound Jar on the desktop: the SDL capture callback (producer) hands PC
// microphone samples to the playback callback (consumer), which feeds the
// sampler on the same thread that mixes it, like the device audio task.
// Nothing is kept beyond the sampler's single in-RAM clip.
struct SimAudio {
  static constexpr uint32_t kRing = 8192;  // power of two
  audio::AudioFeedback* feedback = nullptr;
  audio::Sampler* sampler = nullptr;
  int16_t ring[kRing]{};
  std::atomic<uint32_t> head{0};
  std::atomic<uint32_t> tail{0};
};

void sdl_capture_callback(void* user, Uint8* stream, int len) {
  auto* a = static_cast<SimAudio*>(user);
  const auto* in = reinterpret_cast<const int16_t*>(stream);
  const std::size_t n = static_cast<std::size_t>(len) / sizeof(int16_t);
  uint32_t h = a->head.load(std::memory_order_relaxed);
  const uint32_t t = a->tail.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n && h - t < SimAudio::kRing; ++i) a->ring[h++ % SimAudio::kRing] = in[i];
  a->head.store(h, std::memory_order_release);
}

void sdl_audio_callback(void* user, Uint8* stream, int len) {
  auto* a = static_cast<SimAudio*>(user);
  if (a->sampler) {
    int16_t block[256];
    uint32_t t = a->tail.load(std::memory_order_relaxed);
    const uint32_t h = a->head.load(std::memory_order_acquire);
    while (t != h) {
      std::size_t k = 0;
      while (k < 256 && t != h) block[k++] = a->ring[t++ % SimAudio::kRing];
      a->sampler->capture(block, k);
    }
    a->tail.store(t, std::memory_order_release);
  }
  a->feedback->render(reinterpret_cast<int16_t*>(stream), static_cast<std::size_t>(len) / sizeof(int16_t));
}

bool open_sim_audio(SimAudio& sim, bool want_mic) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
  SDL_AudioSpec want{};
  want.freq = static_cast<int>(audio::kSampleRateHz);
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 256;
  want.callback = sdl_audio_callback;
  want.userdata = &sim;
  const SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
  if (dev == 0) return false;
  if (want_mic && sim.sampler) {
    SDL_AudioSpec cap = want;
    cap.callback = sdl_capture_callback;
    const SDL_AudioDeviceID mic = SDL_OpenAudioDevice(nullptr, 1, &cap, nullptr, 0);
    if (mic != 0) {
      sim.sampler->set_mic_hardware(true);
      SDL_PauseAudioDevice(mic, 0);
    } else {
      std::printf("sim microphone unavailable (%s); Sound Jar uses built-in sounds\n", SDL_GetError());
    }
  }
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

class FeedMonsterPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using feed_monster::FeedMonsterGame;
    using feed_monster::Phase;
    const auto& g = a.feed_monster();
    if (g.phase() != last_) {
      last_ = g.phase();
      next_ = now + 300;
      step_ = 0;
    }
    if (now < next_ || g.phase() != Phase::PLAY) return;
    if (step_++ == 0) {
      tap(a, FeedMonsterGame::kRequestBadge, now);
    } else {
      tap(a, FeedMonsterGame::kFoodBowl, now);
    }
    next_ = now + 450;
  }

 private:
  feed_monster::Phase last_ = feed_monster::Phase::INTRO;
  uint32_t next_ = 0;
  uint32_t step_ = 0;
};

class AnimalSoundsPlayer {
 public:
  void tick(Arcade& a, uint32_t now) {
    using animal_sounds::Phase;
    const auto& g = a.animal_sounds();
    if (g.phase() != last_) {
      last_ = g.phase();
      next_ = now + 500;
      wrong_tapped_ = false;
    }
    if (now < next_ || g.phase() != Phase::PLAY) return;
    if (!wrong_tapped_) {
      for (std::size_t i = 0; i < g.card_count(); ++i) {
        if (static_cast<int>(i) != g.target_index()) {
          tap(a, g.card(i).rect, now);
          wrong_tapped_ = true;
          next_ = now + 650;
          return;
        }
      }
    }
    const int target = g.target_index();
    if (target >= 0) tap(a, g.card(static_cast<std::size_t>(target)).rect, now);
    next_ = now + 900;
  }

 private:
  animal_sounds::Phase last_ = animal_sounds::Phase::INTRO;
  uint32_t next_ = 0;
  bool wrong_tapped_ = false;
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

  const bool animals = opt.game == GameId::ANIMAL_SOUNDS;
  const bool feed = opt.game == GameId::FEED_MONSTER;
  const bool hatch = opt.game == GameId::HATCH_A_PET;
  const bool piano = opt.game == GameId::RAINBOW_PIANO;
  const bool robo = opt.game == GameId::ROBO_BUDDY;
  const uint32_t seed = opt.seed ? opt.seed : 1234;
  static ConsoleFeedback feedback;
  feedback.quiet = true;
  static Arcade arcade(feedback, seed);
  arcade.create(lv_screen_active());
  ColorPopPlayer pop_player;
  AnimalSoundsPlayer animal_player;
  FeedMonsterPlayer feed_player;
  HatchAPetPlayer hatch_player;
  RainbowPianoPlayer piano_player;
  RoboBuddyPlayer robo_player;

  ShotPlan plan;
  plan.dir = opt.shots;
  plan.prefix = animals ? "animal_sounds_" : (feed ? "feed_monster_" : (hatch ? "hatch_a_pet_" : (piano ? "rainbow_piano_" : (robo ? "robo_buddy_" : "color_pop_"))));
  if (animals) {
    plan.shots = {{"00_menu", false}, {"01_intro", false}, {"02_play_two", false},
                  {"03_wrong_tap_wiggle", false}, {"04_celebrate", false}, {"05_play_four", false}};
  } else if (feed) {
    plan.shots = {{"00_menu", false}, {"01_intro", false}, {"02_play", false},
                  {"03_chew", false}, {"04_celebrate", false}, {"05_next_request", false}};
  } else if (hatch) {
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
  const Rect tile = SceneManager::kTileRects[feed ? 2 : (animals ? 1 : (robo ? 3 : (piano ? 2 : (hatch ? 1 : 0))))];
  const uint32_t frames = opt.seconds * 1000 / opt.frame_ms;
  std::vector<double> render_us, sync_us;
  std::vector<uint64_t> pixels;
  render_us.reserve(frames);
  sync_us.reserve(frames);
  bool launched = false;
  bool animal_page_selected = false;
  int pop_phase = -1, feed_phase = -1, hatch_phase = -1, piano_phase = -1, robo_phase = -1, last_wiggle = -1;
  int animal_phase = -1;
  uint32_t phase_start = 0, wiggle_start = 0;

  using clock = std::chrono::steady_clock;
  for (uint32_t f = 0; f < frames; ++f) {
    g_sim_now += opt.frame_ms;
    if ((animals || feed) && !animal_page_selected && g_sim_now >= launch_at / 2) {
      tap(arcade, SceneManager::kNextPageButton, g_sim_now);
      animal_page_selected = true;
    }
    if (!launched && g_sim_now >= launch_at) {
      tap(arcade, tile, g_sim_now);
      launched = true;
    }
    const auto t0 = clock::now();
    if (arcade.layer() != Arcade::Layer::MENU) {
      if (animals) {
        animal_player.tick(arcade, g_sim_now);
      } else if (feed) {
        feed_player.tick(arcade, g_sim_now);
      } else if (hatch) {
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
    if (animals) {
      using animal_sounds::Phase;
      const auto& g = arcade.animal_sounds();
      if (arcade.layer() != Arcade::Layer::ANIMAL_SOUNDS) continue;
      if (static_cast<int>(g.phase()) != animal_phase) {
        animal_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 100, state.frame);
      plan.maybe(2, g.phase() == Phase::PLAY &&
                         g.card_count() == animal_sounds::AnimalSoundsGame::kMinCards && in_phase >= 100,
                 state.frame);
      plan.maybe(3, g.wiggling() >= 0 && g.hint_active(), state.frame);
      plan.maybe(4, g.phase() == Phase::CELEBRATE && in_phase >= 100, state.frame);
      plan.maybe(5, g.phase() == Phase::PLAY &&
                         g.card_count() == animal_sounds::AnimalSoundsGame::kMaxCards && in_phase >= 100,
                 state.frame);
    } else if (feed) {
      using feed_monster::Phase;
      const auto& g = arcade.feed_monster();
      if (arcade.layer() != Arcade::Layer::FEED_MONSTER) continue;
      if (static_cast<int>(g.phase()) != feed_phase) {
        feed_phase = static_cast<int>(g.phase());
        phase_start = g_sim_now;
      }
      const uint32_t in_phase = g_sim_now - phase_start;
      plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 100, state.frame);
      plan.maybe(2, g.phase() == Phase::PLAY && in_phase >= 100, state.frame);
      plan.maybe(3, g.phase() == Phase::PLAY && g.chewing(), state.frame);
      plan.maybe(4, g.phase() == Phase::CELEBRATE && in_phase >= 100, state.frame);
      plan.maybe(5, g.phase() == Phase::PLAY && g.rounds_completed() > 0 && in_phase >= 100, state.frame);
    } else if (hatch) {
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
  const char* name = animals ? "animal_sounds" : (feed ? "feed_monster" : (hatch ? "hatch_a_pet" : (piano ? "rainbow_piano" : (robo ? "robo_buddy" : "color_pop"))));
  const uint32_t progress = animals ? arcade.animal_sounds().rounds_completed()
                                  : (feed ? arcade.feed_monster().rounds_completed()
                                  : (hatch ? arcade.hatch_a_pet().hatch_count()
                                  : (piano ? arcade.rainbow_piano().tunes_completed()
                                           : (robo ? arcade.robo_buddy().reactions()
                                                   : arcade.color_pop().rounds_completed()))));
  const hatch_a_pet::HatchAPetView& hv = arcade.hatch_a_pet_view();

  std::printf("\n%s LVGL benchmark via Arcade (HOST DESKTOP, not device)\n", name);
  std::printf("  seed=%u frame_ms=%u sim_seconds=%u frames=%u rendered=%zu %s=%u cues=%u\n", seed, opt.frame_ms,
              opt.seconds, frames, rendered, animals || feed ? "rounds" : (hatch ? "hatches" : (piano ? "tunes" : (robo ? "reactions" : "rounds"))),
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

// Scripted Sound Jar session. A real audio::Sampler stands in for the device
// audio task: mix() runs every frame and a synthetic "voice" (a gliding
// tone, never a recording) is captured while the jar is listening.
class SoundJarPlayer {
 public:
  enum Step : int {
    WAIT_PLAY, RECORD, LISTEN, STOP, CELEBRATE, CAUGHT, TAP_PAD, BACKWARDS, ECHO_ROBOT, BEND_HOLD, BEND_SLIDE,
    BEND_RELEASE, DONE
  };
  int step() const { return step_; }
  uint32_t in_step(uint32_t now) const { return now - step_at_; }

  void tick(Arcade& a, uint32_t now) {
    using sound_jar::Phase;
    using sound_jar::SoundJarGame;
    const auto& g = a.sound_jar();
    if (holding_) a.pointer(true, hold_x_, hold_y_, now);  // a finger keeps reporting
    // Queued taps go out one per 150 ms (SceneManager debounces faster taps)
    // and hold the script until they have all landed.
    if (queued_ > 0) {
      if (now - last_tap_ >= 150) {
        press(a, taps_[0], now);
        release(a, now);
        last_tap_ = now;
        for (std::size_t i = 1; i < queued_; ++i) taps_[i - 1] = taps_[i];
        --queued_;
      }
      return;
    }
    const uint32_t t = in_step(now);
    switch (step_) {
      case WAIT_PLAY: if (g.phase() == Phase::PLAY && t >= 1600) next(now); break;
      case RECORD: press(a, SoundJarGame::kRecordButton, now); release(a, now); next(now); break;
      case LISTEN: if (g.phase() == Phase::RECORDING && t >= 1300) next(now); break;
      case STOP: press(a, SoundJarGame::kRecordButton, now); release(a, now); next(now); break;
      case CELEBRATE: if (g.phase() == Phase::CELEBRATE) next(now); break;
      case CAUGHT:
        if (g.phase() != Phase::PLAY) {
          play_at_ = 0;
        } else if (play_at_ == 0) {
          play_at_ = now;
        } else if (now - play_at_ >= 600) {
          next(now);
        }
        break;
      case TAP_PAD:
        if (t == 0 || !holding_) press(a, SoundJarGame::kPads[3], now);
        if (t >= 150) { release(a, now); next(now); }
        break;
      case BACKWARDS:
        if (t >= 500) { queue(SoundJarGame::kEffects[0]); queue(SoundJarGame::kJar); next(now); }
        break;
      case ECHO_ROBOT:
        if (t >= 900) {
          queue(SoundJarGame::kEffects[0]);  // backwards off again
          queue(SoundJarGame::kEffects[1]);
          queue(SoundJarGame::kEffects[2]);
          queue(SoundJarGame::kJar);
          next(now);
        }
        break;
      case BEND_HOLD:
        if (t >= 1000 && !holding_) press(a, SoundJarGame::kPads[0], now);
        if (t >= 1400) next(now);
        break;
      case BEND_SLIDE: {
        const Rect& from = SoundJarGame::kPads[0];
        const Rect& to = SoundJarGame::kPads[4];
        const uint32_t k = std::min<uint32_t>(t, 800);
        hold_x_ = static_cast<int16_t>(mid_x(from) + (mid_x(to) - mid_x(from)) * static_cast<int>(k) / 800);
        if (t >= 1100) next(now);
        break;
      }
      case BEND_RELEASE: release(a, now); next(now); break;
      default: break;
    }
  }

 private:
  void next(uint32_t now) { ++step_; step_at_ = now; }
  void queue(const Rect& r) {
    if (queued_ < 8) taps_[queued_++] = r;
  }
  void press(Arcade& a, const Rect& r, uint32_t now) {
    hold_x_ = mid_x(r);
    hold_y_ = mid_y(r);
    holding_ = true;
    a.pointer(true, hold_x_, hold_y_, now);
  }
  void release(Arcade& a, uint32_t now) {
    if (!holding_) return;
    holding_ = false;
    a.pointer(false, hold_x_, hold_y_, now);
  }
  int step_ = WAIT_PLAY;
  uint32_t step_at_ = 0;
  uint32_t play_at_ = 0;
  Rect taps_[8]{};
  std::size_t queued_ = 0;
  uint32_t last_tap_ = 0;
  bool holding_ = false;
  int16_t hold_x_ = 0, hold_y_ = 0;
};

int run_sound_jar_bench(const Options& opt) {
  static BenchState state{};
  g_bench = &state;
  lv_tick_set_cb(sim_tick);
  lv_display_t* disp = lv_display_create(kW, kH);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, state.buf1, state.buf2, sizeof(state.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, bench_flush);

  static std::vector<int16_t> clip(audio::Sampler::kMaxSamples), record(audio::Sampler::kMaxSamples),
      echo(audio::Sampler::kEchoSamples);
  static audio::Sampler sampler(clip.data(), record.data(), echo.data());
  sampler.set_mic_hardware(true);
  const uint32_t seed = opt.seed ? opt.seed : 1234;
  static ConsoleFeedback feedback;
  feedback.quiet = true;
  static Arcade arcade(feedback, seed);
  arcade.attach_sampler(&sampler);
  arcade.create(lv_screen_active());
  SoundJarPlayer player;

  ShotPlan plan;
  plan.dir = opt.shots;
  plan.prefix = "sound_jar_";
  plan.shots = {{"00_menu", false},      {"01_intro", false},      {"02_empty_jar", false},
                {"03_recording", false}, {"04_celebrate", false},  {"05_caught", false},
                {"06_pad_glow", false},  {"07_backwards", false},  {"08_echo_robot", false},
                {"09_bend", false}};

  const uint32_t launch_at = 600;
  const uint32_t frames = opt.seconds * 1000 / opt.frame_ms;
  const std::size_t block = static_cast<std::size_t>(opt.frame_ms) * audio::kSampleRateHz / 1000u;
  std::vector<int16_t> mic(block), out(block);
  std::vector<double> render_us, sync_us, mix_us;
  std::vector<uint64_t> pixels;
  bool paged = false, launched = false;
  double voice_phase = 0.0;
  uint32_t voice_n = 0;
  int32_t peak_out = 0;
  int max_bend = -1;
  int last_phase = -1;
  uint32_t phase_start = 0;

  using clock = std::chrono::steady_clock;
  for (uint32_t f = 0; f < frames; ++f) {
    g_sim_now += opt.frame_ms;
    if (!paged && g_sim_now >= launch_at / 2) {
      tap(arcade, SceneManager::kNextPageButton, g_sim_now);
      paged = true;
    }
    if (!launched && g_sim_now >= launch_at) {
      tap(arcade, SceneManager::kTileRects[3], g_sim_now);
      launched = true;
    }
    const auto t0 = clock::now();
    if (arcade.layer() == Arcade::Layer::SOUND_JAR) player.tick(arcade, g_sim_now);
    arcade.tick(g_sim_now);
    const auto t1 = clock::now();
    // Audio task stand-in: synthetic gliding "voice" with a soft envelope.
    if (sampler.recording()) {
      for (std::size_t i = 0; i < block; ++i, ++voice_n) {
        const double s = static_cast<double>(voice_n) / audio::kSampleRateHz;
        const double hz = 180.0 + 120.0 * std::sin(6.2831853 * 1.5 * s);
        voice_phase += 6.2831853 * hz / audio::kSampleRateHz;
        const double env = std::min(1.0, s * 8.0) * (0.6 + 0.4 * std::sin(6.2831853 * 3.0 * s));
        mic[i] = static_cast<int16_t>(9000.0 * env * (std::sin(voice_phase) + 0.3 * std::sin(2.0 * voice_phase)));
      }
      sampler.capture(mic.data(), block);
    }
    std::fill(out.begin(), out.end(), int16_t{0});
    const auto m0 = clock::now();
    sampler.mix(out.data(), block);
    mix_us.push_back(std::chrono::duration<double, std::micro>(clock::now() - m0).count());
    for (int16_t v : out) peak_out = std::max<int32_t>(peak_out, v < 0 ? -v : v);
    max_bend = std::max(max_bend, arcade.sound_jar().bend_position());
    state.frame_pixels = 0;
    lv_timer_handler();
    const auto t2 = clock::now();
    sync_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    if (state.frame_pixels > 0) {
      render_us.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
      pixels.push_back(state.frame_pixels);
    }

    if (plan.dir.empty()) continue;
    using sound_jar::Phase;
    using S = SoundJarPlayer;
    const auto& g = arcade.sound_jar();
    plan.maybe(0, arcade.layer() == Arcade::Layer::MENU && g_sim_now >= launch_at - 2 * opt.frame_ms, state.frame);
    if (arcade.layer() != Arcade::Layer::SOUND_JAR) continue;
    if (static_cast<int>(g.phase()) != last_phase) {
      last_phase = static_cast<int>(g.phase());
      phase_start = g_sim_now;
    }
    const uint32_t in_phase = g_sim_now - phase_start;
    const uint32_t in_step = player.in_step(g_sim_now);
    plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 300, state.frame);
    plan.maybe(2, player.step() == S::WAIT_PLAY && g.phase() == Phase::PLAY && in_step >= 1200, state.frame);
    plan.maybe(3, g.phase() == Phase::RECORDING && in_phase >= 800, state.frame);
    plan.maybe(4, g.phase() == Phase::CELEBRATE && in_phase >= 300, state.frame);
    plan.maybe(5, player.step() == S::CAUGHT && g.phase() == Phase::PLAY && in_phase >= 300, state.frame);
    plan.maybe(6, player.step() == S::TAP_PAD && in_step >= 100, state.frame);
    plan.maybe(7, player.step() == S::ECHO_ROBOT && in_step >= 200, state.frame);
    plan.maybe(8, player.step() == S::BEND_HOLD && in_step >= 700, state.frame);
    plan.maybe(9, player.step() == S::BEND_SLIDE && in_step >= 600, state.frame);
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
  const std::size_t rendered = render_us.size();
  const double avg_px = rendered ? static_cast<double>(px_sum) / static_cast<double>(rendered) : 0.0;
  const double render_avg = rendered ? render_sum / static_cast<double>(rendered) : 0.0;
  const auto& g = arcade.sound_jar();
  const uint32_t progress = g.sounds_caught();
  std::printf("\nsound_jar LVGL benchmark via Arcade (HOST DESKTOP, not device; synthetic tone, no microphone)\n");
  std::printf("  seed=%u frame_ms=%u sim_seconds=%u frames=%u rendered=%zu sounds_caught=%u plays=%u steps=%d/%d\n",
              seed, opt.frame_ms, opt.seconds, frames, rendered, progress, g.plays(), player.step(),
              static_cast<int>(SoundJarPlayer::DONE));
  std::printf("  clip samples=%u peak_out=%d commands_dropped=%u effects=0x%x max_bend=%d\n", sampler.clip_samples(),
              static_cast<int>(peak_out), sampler.commands_dropped(), static_cast<unsigned>(g.effects()), max_bend);
  std::printf("  render us/frame: avg=%.1f p50=%.1f p95=%.1f max=%.1f\n", render_avg, percentile(render_us, 0.5),
              percentile(render_us, 0.95), percentile(render_us, 1.0));
  std::printf("  sampler mix us/%zu-sample block: p50=%.1f p95=%.1f max=%.1f\n", block, percentile(mix_us, 0.5),
              percentile(mix_us, 0.95), percentile(mix_us, 1.0));
  std::printf("  logic+view sync us/frame: p95=%.2f max=%.2f\n", percentile(sync_us, 0.95), percentile(sync_us, 1.0));
  std::printf("  pixels/rendered frame: avg=%.0f (%.1f%% of screen) max=%llu\n", avg_px, 100.0 * avg_px / (kW * kH),
              static_cast<unsigned long long>(px_max));
  std::printf("  lvgl heap: used=%zu max_used=%zu total=%zu frag=%u%%\n", mem.total_size - mem.free_size,
              mem.max_used, mem.total_size, static_cast<unsigned>(mem.frag_pct));
  std::printf("JSON {\"engine\":\"lvgl-9.5.0\",\"platform\":\"host-desktop\",\"game\":\"sound_jar\",\"seed\":%u,"
              "\"frame_ms\":%u,\"frames\":%u,\"rendered\":%zu,\"render_us_avg\":%.1f,\"render_us_p95\":%.1f,"
              "\"render_us_max\":%.1f,\"sync_us_max\":%.1f,\"mix_us_max\":%.1f,\"px_avg\":%.0f,\"px_max\":%llu,"
              "\"heap_max_used\":%zu,\"progress\":%u,\"plays\":%u}\n",
              seed, opt.frame_ms, frames, rendered, render_avg, percentile(render_us, 0.95),
              percentile(render_us, 1.0), percentile(sync_us, 1.0), percentile(mix_us, 1.0), avg_px,
              static_cast<unsigned long long>(px_max), mem.max_used, progress, g.plays());
  const bool ok = progress > 0 && player.step() == SoundJarPlayer::DONE && peak_out > 0 &&
                  g.effects() == (ISampler::kFxEcho | ISampler::kFxRobot) && max_bend >= 300;
  return ok ? 0 : 1;
}

// Scripted Beat Sharks session: builds a beat by tapping cells (cycling
// through each row's three sounds), records a synthetic "voice" into the
// top row, fills every column for the full-ocean wave, then speeds up.
class BeatSharksPlayer {
 public:
  enum Step : int { WAIT_PLAY, PATTERN, GROOVE, RECORD, LISTEN, CAUGHT, FILL, OCEAN, TEMPO, STOP, DONE };
  int step() const { return step_; }
  uint32_t in_step(uint32_t now) const { return now - step_at_; }

  void tick(Arcade& a, uint32_t now) {
    using beat_sharks::BeatSharksGame;
    using beat_sharks::Phase;
    const auto& g = a.beat_sharks();
    if (queued_ > 0) {  // one tap per 150 ms (SceneManager debounces faster taps)
      if (now - last_tap_ >= 150) {
        tap_at(a, taps_[0], now);
        last_tap_ = now;
        for (std::size_t i = 1; i < queued_; ++i) taps_[i - 1] = taps_[i];
        --queued_;
      }
      return;
    }
    const uint32_t t = in_step(now);
    switch (step_) {
      case WAIT_PLAY: if (g.phase() == Phase::PLAY && g.playing() && t >= 1500) next(now); break;
      case PATTERN:
        queue(BeatSharksGame::cell_rect(1, 1));  // clap
        queue(BeatSharksGame::cell_rect(1, 2), 2);  // woodblock
        queue(BeatSharksGame::cell_rect(2, 4), 3);  // bongo
        next(now);
        break;
      case GROOVE: if (t >= 2500) next(now); break;
      case RECORD: queue(BeatSharksGame::kRecordButton); next(now); break;
      case LISTEN:
        if (g.phase() == Phase::CELEBRATE) next(now);
        else if (g.phase() == Phase::RECORDING && t >= 2500) queue(BeatSharksGame::kRecordButton);
        break;
      case CAUGHT: if (g.phase() == Phase::PLAY && t >= 1800) next(now); break;
      case FILL: queue(BeatSharksGame::cell_rect(1, 5)); next(now); break;
      case OCEAN: if (g.phase() == Phase::PLAY && t >= 1200) next(now); break;
      case TEMPO:
        if (!tempo_tapped_) {
          queue(BeatSharksGame::kTempoButton);
          tempo_tapped_ = true;
        } else if (t >= 2000) {
          next(now);
        }
        break;
      case STOP:
        if (t >= 200) {
          queue(BeatSharksGame::kPlayButton);
          next(now);
        }
        break;
      default: break;
    }
  }

 private:
  void next(uint32_t now) { ++step_; step_at_ = now; }
  void queue(const Rect& r, int times = 1) {
    for (int i = 0; i < times && queued_ < 16; ++i) taps_[queued_++] = r;
  }
  static void tap_at(Arcade& a, const Rect& r, uint32_t now) {
    a.pointer(true, mid_x(r), mid_y(r), now);
    a.pointer(false, mid_x(r), mid_y(r), now);
  }
  int step_ = WAIT_PLAY;
  uint32_t step_at_ = 0;
  Rect taps_[16]{};
  std::size_t queued_ = 0;
  uint32_t last_tap_ = 0;
  bool tempo_tapped_ = false;
};

int run_beat_sharks_bench(const Options& opt) {
  static BenchState state{};
  g_bench = &state;
  lv_tick_set_cb(sim_tick);
  lv_display_t* disp = lv_display_create(kW, kH);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, state.buf1, state.buf2, sizeof(state.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, bench_flush);

  static std::vector<int16_t> clip(audio::Sampler::kMaxSamples), record(audio::Sampler::kMaxSamples),
      echo(audio::Sampler::kEchoSamples);
  static audio::Sampler sampler(clip.data(), record.data(), echo.data());
  sampler.set_mic_hardware(true);
  const uint32_t seed = opt.seed ? opt.seed : 1234;
  static ConsoleFeedback feedback;
  feedback.quiet = true;
  static Arcade arcade(feedback, seed);
  arcade.attach_sampler(&sampler);
  arcade.create(lv_screen_active());
  BeatSharksPlayer player;

  ShotPlan plan;
  plan.dir = opt.shots;
  plan.prefix = "beat_sharks_";
  plan.shots = {{"00_menu", false},    {"01_intro", false},   {"02_starter", false}, {"03_pattern", false},
                {"04_recording", false}, {"05_new_sound", false}, {"06_caught", false}, {"07_full_ocean", false},
                {"08_rabbit", false},  {"09_stopped", false}};

  // Beat Sharks is the first tile on launcher page 3.
  const uint32_t launch_at = 900;
  const uint32_t frames = opt.seconds * 1000 / opt.frame_ms;
  const std::size_t block = static_cast<std::size_t>(opt.frame_ms) * audio::kSampleRateHz / 1000u;
  std::vector<int16_t> mic(block), out(block);
  std::vector<double> render_us, sync_us, mix_us;
  std::vector<uint64_t> pixels;
  int pages = 0;
  bool launched = false;
  double voice_phase = 0.0;
  uint32_t voice_n = 0;
  int32_t peak_out = 0;
  int last_phase = -1;
  uint32_t phase_start = 0;
  bool saw_full_ocean = false;

  using clock = std::chrono::steady_clock;
  for (uint32_t f = 0; f < frames; ++f) {
    g_sim_now += opt.frame_ms;
    if (pages < 2 && g_sim_now >= 300u * static_cast<uint32_t>(pages + 1)) {
      tap(arcade, SceneManager::kNextPageButton, g_sim_now);
      ++pages;
    }
    if (!launched && g_sim_now >= launch_at) {
      tap(arcade, SceneManager::kTileRects[0], g_sim_now);
      launched = true;
    }
    const auto t0 = clock::now();
    if (arcade.layer() == Arcade::Layer::BEAT_SHARKS) player.tick(arcade, g_sim_now);
    arcade.tick(g_sim_now);
    const auto t1 = clock::now();
    if (sampler.recording()) {  // synthetic gliding tone, never a real recording
      for (std::size_t i = 0; i < block; ++i, ++voice_n) {
        const double s = static_cast<double>(voice_n) / audio::kSampleRateHz;
        const double hz = 220.0 + 90.0 * std::sin(6.2831853 * 2.0 * s);
        voice_phase += 6.2831853 * hz / audio::kSampleRateHz;
        const double env = std::min(1.0, s * 8.0);
        mic[i] = static_cast<int16_t>(9000.0 * env * std::sin(voice_phase));
      }
      sampler.capture(mic.data(), block);
    }
    std::fill(out.begin(), out.end(), int16_t{0});
    const auto m0 = clock::now();
    sampler.mix(out.data(), block);
    mix_us.push_back(std::chrono::duration<double, std::micro>(clock::now() - m0).count());
    for (int16_t v : out) peak_out = std::max<int32_t>(peak_out, v < 0 ? -v : v);
    saw_full_ocean = saw_full_ocean || arcade.beat_sharks().celebration() == beat_sharks::Celebration::FULL_OCEAN;
    state.frame_pixels = 0;
    lv_timer_handler();
    const auto t2 = clock::now();
    sync_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    if (state.frame_pixels > 0) {
      render_us.push_back(std::chrono::duration<double, std::micro>(t2 - t1).count());
      pixels.push_back(state.frame_pixels);
    }

    if (plan.dir.empty()) continue;
    using beat_sharks::Celebration;
    using beat_sharks::Phase;
    using S = BeatSharksPlayer;
    const auto& g = arcade.beat_sharks();
    plan.maybe(0, arcade.layer() == Arcade::Layer::MENU && g_sim_now >= launch_at - 2 * opt.frame_ms, state.frame);
    if (arcade.layer() != Arcade::Layer::BEAT_SHARKS) continue;
    if (static_cast<int>(g.phase()) != last_phase) {
      last_phase = static_cast<int>(g.phase());
      phase_start = g_sim_now;
    }
    const uint32_t in_phase = g_sim_now - phase_start;
    const uint32_t in_step = player.in_step(g_sim_now);
    plan.maybe(1, g.phase() == Phase::INTRO && in_phase >= 300, state.frame);
    plan.maybe(2, player.step() == S::WAIT_PLAY && g.playing() && in_step >= 1200, state.frame);
    plan.maybe(3, player.step() == S::GROOVE && in_step >= 2000, state.frame);
    plan.maybe(4, g.phase() == Phase::RECORDING && in_phase >= 900, state.frame);
    plan.maybe(5, g.celebration() == Celebration::NEW_SOUND && in_phase >= 300, state.frame);
    plan.maybe(6, player.step() == S::CAUGHT && g.phase() == Phase::PLAY && in_step >= 1500, state.frame);
    plan.maybe(7, g.celebration() == Celebration::FULL_OCEAN && in_phase >= 250, state.frame);
    plan.maybe(8, player.step() == S::TEMPO && in_step >= 1500, state.frame);
    plan.maybe(9, player.step() == S::DONE && !g.playing() && in_step >= 300, state.frame);
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
  const std::size_t rendered = render_us.size();
  const double avg_px = rendered ? static_cast<double>(px_sum) / static_cast<double>(rendered) : 0.0;
  const double render_avg = rendered ? render_sum / static_cast<double>(rendered) : 0.0;
  const auto& g = arcade.beat_sharks();
  std::printf("\nbeat_sharks LVGL benchmark via Arcade (HOST DESKTOP, not device; synthetic tone, no microphone)\n");
  std::printf("  seed=%u frame_ms=%u sim_seconds=%u frames=%u rendered=%zu steps_played=%u hits=%u caught=%u "
              "tempo=%u script=%d/%d full_ocean=%d\n",
              seed, opt.frame_ms, opt.seconds, frames, rendered, g.steps_played(), g.hits(), g.sounds_caught(),
              static_cast<unsigned>(g.tempo()), player.step(), static_cast<int>(BeatSharksPlayer::DONE),
              saw_full_ocean ? 1 : 0);
  std::printf("  clip samples=%u peak_out=%d commands_dropped=%u\n", sampler.clip_samples(),
              static_cast<int>(peak_out), sampler.commands_dropped());
  std::printf("  render us/frame: avg=%.1f p50=%.1f p95=%.1f max=%.1f\n", render_avg, percentile(render_us, 0.5),
              percentile(render_us, 0.95), percentile(render_us, 1.0));
  std::printf("  sampler mix us/%zu-sample block: p50=%.1f p95=%.1f max=%.1f\n", block, percentile(mix_us, 0.5),
              percentile(mix_us, 0.95), percentile(mix_us, 1.0));
  std::printf("  logic+view sync us/frame: p95=%.2f max=%.2f\n", percentile(sync_us, 0.95), percentile(sync_us, 1.0));
  std::printf("  pixels/rendered frame: avg=%.0f (%.1f%% of screen) max=%llu\n", avg_px, 100.0 * avg_px / (kW * kH),
              static_cast<unsigned long long>(px_max));
  std::printf("  lvgl heap: used=%zu max_used=%zu total=%zu frag=%u%%\n", mem.total_size - mem.free_size,
              mem.max_used, mem.total_size, static_cast<unsigned>(mem.frag_pct));
  std::printf("JSON {\"engine\":\"lvgl-9.5.0\",\"platform\":\"host-desktop\",\"game\":\"beat_sharks\",\"seed\":%u,"
              "\"frame_ms\":%u,\"frames\":%u,\"rendered\":%zu,\"render_us_avg\":%.1f,\"render_us_p95\":%.1f,"
              "\"render_us_max\":%.1f,\"sync_us_max\":%.1f,\"mix_us_max\":%.1f,\"px_avg\":%.0f,\"px_max\":%llu,"
              "\"heap_max_used\":%zu,\"steps\":%u,\"hits\":%u,\"caught\":%u}\n",
              seed, opt.frame_ms, frames, rendered, render_avg, percentile(render_us, 0.95),
              percentile(render_us, 1.0), percentile(sync_us, 1.0), percentile(mix_us, 1.0), avg_px,
              static_cast<unsigned long long>(px_max), mem.max_used, g.steps_played(), g.hits(), g.sounds_caught());
  const bool ok = player.step() == BeatSharksPlayer::DONE && g.sounds_caught() > 0 && saw_full_ocean &&
                  g.hits() > 0 && peak_out > 0 && !g.playing() && g.tempo() == 2;
  return ok ? 0 : 1;
}

// -------------------------------------------------------------- interactive
int run_interactive(const Options& opt) {
  lv_display_t* disp = lv_sdl_window_create(kW, kH);
  lv_sdl_window_set_zoom(disp, static_cast<float>(opt.zoom));
  lv_sdl_window_set_title(disp, "Little Arcade sim (desktop, not device)");
  lv_tick_set_cb(SDL_GetTicks);

  static SpeakerFeedback feedback;
  static std::vector<int16_t> clip(audio::Sampler::kMaxSamples), record(audio::Sampler::kMaxSamples),
      echo(audio::Sampler::kEchoSamples);
  static audio::Sampler sampler(clip.data(), record.data(), echo.data());
  static SimAudio sim;
  sim.feedback = &feedback.audio;
  sim.sampler = &sampler;
  feedback.audio.attach_sampler(&sampler);
  const bool audio_ok = !opt.mute && open_sim_audio(sim, opt.mic);
  if (!audio_ok) {
    std::printf("sim audio off (%s)\n", opt.mute ? "--mute" : SDL_GetError());
  }
  const uint32_t seed = opt.seed ? opt.seed : static_cast<uint32_t>(SDL_GetTicks() ^ 0xA5A5u) | 1u;
  static Arcade arcade(feedback, seed, static_cast<uint8_t>(opt.volume));
  arcade.attach_sampler(audio_ok ? &sampler : nullptr);
  arcade.create(lv_screen_active());
  int applied_level = -1;
  std::printf(
      "Little Arcade sim: click = touch. Keys: R/B/G/Y colours, A again, H help, J yes, N no, X next,\n"
      "  Backspace back, C colors, E eggs, K counting, M music, T robot, Esc quit. Sound is muted: hold the Mic corner 5 s for parent settings.\n");

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
              {SDL_SCANCODE_K, Intent::COUNTING, false},  {SDL_SCANCODE_M, Intent::MUSIC, false},
              {SDL_SCANCODE_T, Intent::ROBOT, false}};
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
    if (arcade.take_mic_changed()) {
      sampler.set_mic_allowed(arcade.mic_enabled());
      std::printf("[settings] parent turned the Sound Jar mic %s (not persisted in the sim)\n",
                  arcade.mic_enabled() ? "on" : "off");
    }
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

// Snapshot mode: any game. Launches it from the launcher (paging as needed),
// replays scripted taps and writes screenshots at fixed times after launch,
// 120 ms and 450 ms after each tap, and optionally after Home + relaunch.
// Prints the LVGL pool high-water mark. Desktop only, not hardware evidence.
int run_snap(const Options& opt) {
  static BenchState state{};
  g_bench = &state;
  lv_tick_set_cb(sim_tick);
  lv_display_t* disp = lv_display_create(kW, kH);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, state.buf1, state.buf2, sizeof(state.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, bench_flush);
  static ConsoleFeedback feedback;
  feedback.quiet = true;
  static Arcade arcade(feedback, opt.seed ? opt.seed : 1234);
  arcade.create(lv_screen_active());

  struct Tap { int x, y; uint32_t ms; };
  std::vector<Tap> taps;
  for (std::size_t p = 0; p < opt.taps.size();) {
    std::size_t e = opt.taps.find(';', p);
    if (e == std::string::npos) e = opt.taps.size();
    Tap t{};
    if (std::sscanf(opt.taps.substr(p, e - p).c_str(), "%d,%d@%u", &t.x, &t.y, &t.ms) == 3) taps.push_back(t);
    p = e + 1;
  }
  std::vector<uint32_t> times = {100, 600, 1500, 3000, 6000};
  for (std::size_t p = 0; p < opt.at.size();) {
    std::size_t e = opt.at.find(',', p);
    if (e == std::string::npos) e = opt.at.size();
    times.push_back(static_cast<uint32_t>(std::strtoul(opt.at.substr(p, e - p).c_str(), nullptr, 10)));
    p = e + 1;
  }
  struct Pending { uint32_t at; std::string name; };
  std::vector<Pending> pending;
  const std::string base = opt.shots + "/" + opt.game_name + "_";
  auto shoot = [&](const std::string& name) {
    const std::string path = base + name + ".bmp";
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(disp);
    std::printf("shot %s %s\n", path.c_str(), write_bmp(path, state.frame) ? "ok" : "FAILED");
  };

  auto launch = [&](uint32_t now) {
    const SceneManager& sm = arcade.scenes();
    for (std::size_t slot = 0; slot < SceneManager::kTilesPerPage; ++slot) {
      const GameEntry* e = sm.tile(slot);
      if (e && e->id == opt.game) {
        tap(arcade, SceneManager::kTileRects[slot], now);
        return true;
      }
    }
    tap(arcade, SceneManager::kNextPageButton, now);
    return false;
  };

  const uint32_t total_ms = opt.seconds * 1000;
  uint32_t launched_at = 0, next_try = 400, home_at = 0, relaunch_at = 0;
  bool launched = false, menu_shot = false, went_home = false, relaunched = false;
  std::size_t next_tap = 0;
  uint32_t lv_max = 0;
  while (g_sim_now < total_ms + 4000) {
    g_sim_now += opt.frame_ms;
    if (!launched && g_sim_now >= next_try) {
      if (!menu_shot) {
        shoot("menu");
        menu_shot = true;
      }
      launched = launch(g_sim_now);
      next_try = g_sim_now + 100;
      if (launched) {
        launched_at = g_sim_now;
        for (uint32_t t : times) pending.push_back({launched_at + t, "t" + std::to_string(t)});
      }
    }
    if (launched && !went_home && next_tap < taps.size() && g_sim_now - launched_at >= taps[next_tap].ms) {
      const Tap& t = taps[next_tap];
      arcade.touch(static_cast<int16_t>(t.x), static_cast<int16_t>(t.y), g_sim_now);
      pending.push_back({g_sim_now + 120, "tap" + std::to_string(next_tap) + "_a"});
      pending.push_back({g_sim_now + 450, "tap" + std::to_string(next_tap) + "_b"});
      ++next_tap;
    }
    if (launched && opt.reenter && !went_home && g_sim_now - launched_at >= total_ms) {
      tap(arcade, kHomeButton, g_sim_now);
      went_home = true;
      home_at = g_sim_now;
    }
    if (went_home && !relaunched && g_sim_now - home_at >= 600) {
      if (launch(g_sim_now)) {
        relaunched = true;
        relaunch_at = g_sim_now;
        pending.push_back({relaunch_at + 1500, "reenter"});
      }
    }
    arcade.tick(g_sim_now);
    lv_timer_handler();
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    lv_max = std::max<uint32_t>(lv_max, static_cast<uint32_t>(mem.total_size - mem.free_size));
    for (auto it = pending.begin(); it != pending.end();) {
      if (g_sim_now >= it->at) {
        shoot(it->name);
        it = pending.erase(it);
      } else {
        ++it;
      }
    }
    if (launched && !opt.reenter && g_sim_now - launched_at >= total_ms && pending.empty()) break;
    if (relaunched && pending.empty()) break;
  }
  std::printf("JSON {\"mode\":\"snap\",\"platform\":\"host-desktop\",\"game\":\"%s\",\"launched\":%s,"
              "\"lv_pool_used_max\":%u}\n",
              opt.game_name.c_str(), launched ? "true" : "false", lv_max);
  return launched ? 0 : 1;
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
      bool found = false;
      for (const GameName& n : kGameNames) {
        if (g == n.name) {
          opt.game = n.id;
          opt.game_name = n.name;
          found = true;
        }
      }
      if (!found) return false;
    } else if (a == "--snap") {
      opt.snap = true;
    } else if (a == "--reenter") {
      opt.reenter = true;
    } else if (a == "--taps" && i + 1 < argc) {
      opt.taps = argv[++i];
    } else if (a == "--at" && i + 1 < argc) {
      opt.at = argv[++i];
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
    } else if (a == "--no-mic") {
      opt.mic = false;
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
        "usage: little_arcade_sim [--zoom N] [--seed N] [--mute] [--no-mic] [--volume 0..6]\n"
        "       little_arcade_sim --bench [--game animal_sounds|beat_sharks|color_pop|feed_monster|hatch_a_pet|rainbow_piano|robo_buddy|sound_jar] [--seconds N] [--seed N]\n"
        "                         [--frame-ms N] [--shots DIR]\n"
        "       little_arcade_sim --snap --game NAME --shots DIR [--seconds N] [--taps \"x,y@ms;...\"] [--at \"ms,...\"] [--reenter]\n");
    return 2;
  }
  SDL_SetMainReady();
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  lv_init();
  if (opt.snap) return run_snap(opt);
  if (opt.bench) {
    if (opt.game == GameId::SOUND_JAR) return run_sound_jar_bench(opt);
    if (opt.game == GameId::BEAT_SHARKS) return run_beat_sharks_bench(opt);
    return run_bench(opt);
  }
  return run_interactive(opt);
}
