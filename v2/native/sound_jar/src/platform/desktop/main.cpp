// Sound Jar desktop build: LVGL 9 + SDL2. Mouse input is touch, including the press-and-hold
// loop and the vertical pitch drag on the pads. Audio: SoundJarScore -> shared VolumeLimiter -> SDL.
#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "audio/sound_jar_score.hpp"
#include "game/sound_jar.hpp"
#include "little_arcade/volume.hpp"
#include "lvgl.h"
#include "ui/asset_pack.hpp"
#include "ui/render_profile.hpp"
#include "ui/sound_jar_view.hpp"

namespace {
using namespace sound_jar;
using little_arcade::Intent;
constexpr int W = kWidth, H = kHeight;
constexpr double kStep = 1.0 / 64;  // scripted/check step; the reference itself uses variable dt (capped at 50 ms)

// ---------------- audio ----------------
struct DesktopAudio {
  SoundJarScore score;
  little_arcade::VolumeLimiter limiter{little_arcade::VolumeLimiter::Config{}};
  SDL_AudioDeviceID device = 0;
  std::array<int32_t, little_arcade::VolumeLimiter::kChunk> mix{};
  static void callback(void* user, Uint8* bytes, int length) {
    auto& a = *static_cast<DesktopAudio*>(user);
    auto* out = reinterpret_cast<int16_t*>(bytes);
    std::size_t n = static_cast<std::size_t>(length) / 2;
    while (n) {
      const std::size_t m = std::min(n, a.mix.size());
      std::fill(a.mix.begin(), a.mix.begin() + m, 0);
      a.score.render_add(a.mix.data(), m);
      a.limiter.process(a.mix.data(), out, m);
      out += m; n -= m;
    }
  }
  bool open(uint8_t level) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want{};
    want.freq = static_cast<int>(SoundJarScore::kSampleRate);
    want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 256;
    want.callback = callback; want.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!device) return false;
    limiter.set_level(level);  // limiter starts muted and ramps; the score is silent until Start is tapped
    SDL_PauseAudioDevice(device, 0);
    return true;
  }
  ~DesktopAudio() { if (device) SDL_CloseAudioDevice(device); }
};

// ---------------- assets ----------------
std::vector<uint8_t> g_pack_bytes;
bool load_pack(AssetPack& pack, std::string path) {
  if (path.empty()) {
    char* base = SDL_GetBasePath();
    path = std::string(base ? base : "") + "sound_jar.pack";
    if (base) SDL_free(base);
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) { std::fprintf(stderr, "Cannot open asset pack %s (build the sound_jar_assets target)\n", path.c_str()); return false; }
  g_pack_bytes.assign(std::istreambuf_iterator<char>(in), {});
  std::string error;
  if (!pack.load(g_pack_bytes.data(), g_pack_bytes.size(), &error)) {
    std::fprintf(stderr, "Bad asset pack %s: %s\n", path.c_str(), error.c_str());
    return false;
  }
  return true;
}

// ---------------- offscreen display ----------------
struct Frame {
  std::array<uint16_t, W * H> pixels{};
  std::array<uint16_t, W * 24> buf1{}, buf2{};
};
Frame* g_frame = nullptr;
uint32_t g_sim_ms = 0;
uint64_t g_flushed = 0;  // pixels sent by flush (== redrawn area), for --dirty-stats
bool g_dirty_stats = false;
std::vector<uint8_t> g_redrawn;  // --verify-redraw: pixels covered by a flushed area this frame
uint32_t sim_clock() { return g_sim_ms; }
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) {
  g_flushed += static_cast<uint64_t>(lv_area_get_width(area)) * lv_area_get_height(area);
  if (!g_redrawn.empty())
    for (int y = area->y1; y <= area->y2; ++y) std::memset(&g_redrawn[y * W + area->x1], 1, area->x2 - area->x1 + 1);
  if (data == reinterpret_cast<uint8_t*>(g_frame->pixels.data())) { lv_display_flush_ready(display); return; }  // direct mode
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

// ---------------- scenario scripts (shared with tools/visual_compare.mjs) ----------------
// One command per line: `step N` (N frames of 1/64 s), `down X Y`, `move X Y`, `up`, `tap X Y`
// (down + up), `auto` (tap the reference autoTarget()), `home`, `reduced 0|1`,
// `intent START|BACK|AGAIN|HELP`. '#' starts a comment.
struct Command { std::string op, word; double a = 0, b = 0; };
bool parse_script(const std::string& path, std::vector<Command>& out) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream ss(line);
    Command c;
    if (!(ss >> c.op)) continue;
    if (c.op == "intent") ss >> c.word;
    else ss >> c.a >> c.b;
    out.push_back(c);
  }
  return true;
}
// Applies a non-step command; returns false for an unknown/unsatisfiable command.
bool apply(Game& g, const Command& c) {
  if (c.op == "down") { g.down(c.a, c.b); return true; }
  if (c.op == "move") { g.move(c.a, c.b); return true; }
  if (c.op == "up") { g.up(); return true; }
  if (c.op == "tap") { g.down(c.a, c.b); g.up(); return true; }
  if (c.op == "home") { g.down(20, 20); g.up(); return true; }
  if (c.op == "reduced") { g.set_reduced(c.a != 0); return true; }
  if (c.op == "auto") {
    double x, y;
    if (!g.auto_target(x, y)) { std::fprintf(stderr, "script: no auto target\n"); return false; }
    g.down(x, y); g.up();
    return true;
  }
  if (c.op == "intent") {
    static const struct { const char* name; Intent intent; } kMap[] = {
        {"START", Intent::START}, {"BACK", Intent::BACK}, {"AGAIN", Intent::AGAIN}, {"HELP", Intent::HELP}};
    for (const auto& m : kMap)
      if (c.word == m.name) { g.intent(m.intent); return true; }
  }
  std::fprintf(stderr, "script: unknown command '%s %s'\n", c.op.c_str(), c.word.c_str());
  return false;
}

const char* phase_name(Phase p) {
  switch (p) {
    case Phase::Start: return "START";
    case Phase::Intro: return "INTRO";
    case Phase::Empty: return "EMPTY";
    case Phase::Countdown: return "COUNTDOWN";
    case Phase::Recording: return "RECORDING";
    case Phase::Processing: return "PROCESSING";
    case Phase::Full: return "FULL";
    case Phase::TooQuiet: return "TOO_QUIET";
  }
  return "?";
}
void dump_state(const Game& g) {
  std::printf("{\"phase\":\"%s\",\"t\":%.4f,\"time\":%.4f,\"amb\":%.4f,\"take\":%d,\"sound\":%d,\"mist\":%d,\"tries\":%d,"
              "\"actions\":%d,\"hint\":%d,\"level\":%.4f,\"lid\":%.4f,\"voices\":%zu,\"parts\":%zu,\"rings\":%zu,\"fx\":[",
              phase_name(g.phase()), g.phase_time(), g.time(), g.amb(), g.take(), g.jar().sound, g.jar().mist, g.tries(),
              g.actions(), g.hint(), g.jar().level, g.lid().u, g.voices().size(), g.particles().size(), g.rings().size());
  for (std::size_t i = 0; i < g.fx().size(); ++i) std::printf("%s%d", i ? "," : "", g.fx()[i]);
  std::printf("],\"order\":[");
  for (std::size_t i = 0; i < g.order().size(); ++i) std::printf("%s%d", i ? "," : "", g.order()[i]);
  double psum = 0;
  for (const Particle& p : g.particles()) psum += p.x + p.y;
  std::printf("],\"psum\":%.4f}\n", psum);
}

struct Offscreen {
  Frame frame;
  lv_display_t* display = nullptr;
  double clock = 0;
  double render_ms_total = 0, render_ms_max = 0;
  int frames = 0;
  bool verify = false;  // --verify-redraw: compare each incremental frame with a full redraw
  long long stale_pixels = 0, resampled_pixels = 0;
  int stale_frames = 0, max_delta = 0;
  std::array<uint16_t, W * H> prev_full{};
  bool has_prev = false;
  std::vector<uint32_t> dirty;  // per rendered frame: redrawn pixels
  explicit Offscreen(bool direct = false) {
    g_frame = &frame;
    lv_tick_set_cb(sim_clock);
    display = lv_display_create(W, H);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    if (direct)  // same render mode as the SDL window (lv_conf.h LV_SDL_RENDER_MODE)
      lv_display_set_buffers(display, frame.pixels.data(), nullptr, sizeof(frame.pixels), LV_DISPLAY_RENDER_MODE_DIRECT);
    else
      lv_display_set_buffers(display, frame.buf1.data(), frame.buf2.data(), sizeof(frame.buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
  }
  void step(Game& g, View& v) {
    g.tick(kStep);
    clock += kStep;
    g_sim_ms = static_cast<uint32_t>(clock * 1000);
    v.sync(g);
    const uint64_t flushed = g_flushed;
    if (verify) g_redrawn.assign(W * H, 0);
    const auto t0 = std::chrono::steady_clock::now();
    lv_refr_now(display);
    dirty.push_back(static_cast<uint32_t>(g_flushed - flushed));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    render_ms_total += ms; render_ms_max = std::max(render_ms_max, ms); ++frames;
    if (verify) {
      const auto incremental = frame.pixels;
      const std::vector<uint8_t> redrawn = g_redrawn;
      g_redrawn.clear();
      lv_obj_invalidate(lv_screen_active());
      lv_refr_now(display);
      const auto full = frame.pixels;
      frame.pixels = incremental;  // keep showing the incremental result so real staleness accumulates
      long long diff = 0, resampled = 0;
      int x0 = W, y0 = H, x1 = -1, y1 = -1, worst = 0;
      auto delta = [](uint16_t a, uint16_t b) {
        return std::max({std::abs((a >> 11) - (b >> 11)) * 8, std::abs(((a >> 5) & 63) - ((b >> 5) & 63)) * 4,
                         std::abs((a & 31) - (b & 31)) * 8});
      };
      for (std::size_t i = 0; i < incremental.size(); ++i)
        if (incremental[i] != full[i]) {
          const int d = delta(incremental[i], full[i]);
          worst = std::max(worst, d);
          if (d <= 48) continue;
          // Stale: differs from a full redraw although no flushed area covered it. Differences inside redrawn
          // areas come from LVGL's transformed-image sampling depending on the clip area's origin.
          if (redrawn[i]) { ++resampled; continue; }
          // Unchanged scene here: a sampling residue left by an earlier partial redraw, not a missed update.
          if (has_prev && delta(full[i], prev_full[i]) <= 48) { ++resampled; continue; }
          ++diff;
          const int x = static_cast<int>(i % W), y = static_cast<int>(i / W);
          x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
        }
      max_delta = std::max(max_delta, worst);
      resampled_pixels += resampled;
      prev_full = full; has_prev = true;
      if (diff) {
        if (stale_frames == 0 && std::getenv("SJ_STALE_DUMP")) {
          const std::string base = std::getenv("SJ_STALE_DUMP");
          Frame f;
          f.pixels = incremental; write_bmp(base + "_incremental.bmp", f);
          f.pixels = full; write_bmp(base + "_full.bmp", f);
        }
        if (stale_frames < 8)
          std::fprintf(stderr, "stale redraw at frame %d: %lld px in [%d,%d..%d,%d]\n", frames, diff, x0, y0, x1, y1);
        stale_pixels += diff; ++stale_frames;
      }
    }
  }
};

int run_script(const std::string& script, const std::string& pack_path, uint32_t seed, bool reduced,
               const std::string& screenshot, bool dump, bool verify, bool direct, bool profile) {
  std::vector<Command> commands;
  if (!parse_script(script, commands)) { std::fprintf(stderr, "Cannot read script %s\n", script.c_str()); return 1; }
  AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  Offscreen screen(direct);
  screen.verify = verify;
  Sound silent;
  Game game(silent, seed);
  game.set_reduced(reduced);
  View view;
  const char* missing = nullptr;
  if (!view.create(lv_screen_active(), pack, &missing)) { std::fprintf(stderr, "Missing sprite %s\n", missing); return 1; }
  view.sync(game);
  lv_refr_now(screen.display);
  for (const Command& c : commands) {
    if (c.op == "step") { for (int i = 0; i < static_cast<int>(c.a); ++i) screen.step(game, view); }
    else if (!apply(game, c)) return 2;
  }
  if (!screenshot.empty()) {
    view.sync(game);
    lv_refr_now(screen.display);
    if (!write_bmp(screenshot, screen.frame)) { std::fprintf(stderr, "Cannot write %s\n", screenshot.c_str()); return 3; }
  }
  if (dump) dump_state(game);
  if (profile) {
    std::vector<sound_jar::LayerCost> costs;
    auto now_us = [] {
      return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch()).count());
    };
    const uint32_t base = sound_jar::profile_layers(screen.display, now_us, costs, 7);
    char line[2048];
    sound_jar::format_profile(line, sizeof line, base, costs, 16);
    std::printf("[profile] %s\n", line);
  }
  if (g_dirty_stats && !screen.dirty.empty()) {
    std::vector<uint32_t> d = screen.dirty;
    std::sort(d.begin(), d.end());
    auto pct = [&](int p) { return 100.0 * d[std::min(d.size() - 1, d.size() * p / 100)] / (W * H); };
    const auto full = std::count_if(d.begin(), d.end(), [](uint32_t v) { return v * 2 > W * H; });
    std::printf("dirty frames=%zu p50=%.1f%% p95=%.1f%% max=%.1f%% over50%%=%lld render avg %.3f ms max %.3f ms\n", d.size(),
                pct(50), pct(95), pct(100), static_cast<long long>(full), screen.render_ms_total / screen.frames,
                screen.render_ms_max);
  }
  if (verify) {
    std::fprintf(stderr, "redraw verification: %d/%d frames with stale pixels, %lld stale px; %lld re-sampled px "
                 "(max channel delta %d)\n",
                 screen.stale_frames, screen.frames, screen.stale_pixels, screen.resampled_pixels, screen.max_delta);
    if (screen.stale_frames) return 4;
  }
  return 0;
}

// Built-in offscreen interaction/render check (ctest): a whole session through the real score.
int run_check(const std::string& pack_path, uint32_t seed, const std::string& screenshot) {
  AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  Offscreen screen;
  SoundJarScore score;  // real score, rendered offline to prove the event path is sound
  Game game(score, seed);
  View view;
  const char* missing = nullptr;
  if (!view.create(lv_screen_active(), pack, &missing)) { std::fprintf(stderr, "Missing sprite %s\n", missing); return 1; }
  std::vector<int32_t> pcm(256);
  int32_t audio_peak = 0;
  auto frames = [&](int n) {
    for (int i = 0; i < n; ++i) {
      screen.step(game, view);
      std::fill(pcm.begin(), pcm.end(), 0);
      score.render_add(pcm.data(), 250);  // 16000 / 64 samples per frame
      for (int k = 0; k < 250; ++k) audio_peak = std::max(audio_peak, std::abs(pcm[k]));
    }
  };
  auto until = [&](Phase p, int max_frames) {
    for (int i = 0; i < max_frames && game.phase() != p; ++i) frames(1);
    return game.phase() == p;
  };
  auto tap = [&](double x, double y) { game.down(x, y); game.up(); };
  auto fail = [&](int code, const char* why) { std::fprintf(stderr, "check failed: %s\n", why); dump_state(game); return code; };
  frames(10);
  if (game.phase() != Phase::Start || audio_peak != 0) return fail(2, "start screen must be silent");
  tap(160, 120);
  if (!until(Phase::Empty, 120)) return fail(3, "intro did not reach the empty jar");
  tap(kRecX, kRecY);
  if (game.phase() != Phase::Countdown) return fail(4, "record tap did not start the countdown");
  if (!until(Phase::Recording, 120)) return fail(5, "countdown did not start recording");
  if (game.take() == kWhisper) return fail(6, "the whisper must be the last take of a round");
  if (!until(Phase::Full, 600)) return fail(9, "first recording did not fill the jar");
  if (game.take() == kWhisper || game.jar().sound != game.take()) return fail(10, "jar does not hold the new take");
  frames(80);
  // Hold pad 2 (> 350 ms loops), slide up to bend, release.
  game.down(pad_x(2), 215);
  frames(30);
  if (!game.pad_looping(2)) return fail(11, "held pad did not loop");
  game.move(pad_x(2), 215 - 40);
  frames(10);
  if (game.hold().bend <= 0) return fail(12, "slide up did not bend the pitch up");
  game.up();
  frames(4);
  if (game.pad_looping(2)) return fail(13, "release did not stop the loop");
  tap(kFx[kEcho].x, kFxY); frames(10);
  tap(kFx[kWobble].x, kFxY); frames(10);
  tap(kFx[kRobot].x, kFxY); frames(30);
  if (game.fx().size() != 2 || game.fx_on(kEcho)) return fail(14, "third effect did not replace the oldest");
  tap(160, 120);
  frames(60);
  for (int i = 0; i < 30; ++i) { tap(80 + (i * 37) % 160, 70 + (i * 13) % 20); frames(1); }  // mash empty space
  if (static_cast<int>(game.particles().size()) > kMaxParticles) return fail(15, "particle cap exceeded");
  frames(20);
  tap(kBinX, kBinY);
  frames(4);
  if (game.phase() != Phase::Empty || game.jar().sound != kFrog) return fail(16, "bin did not empty the jar");
  frames(60);
  for (int k = 0; k < 2; ++k) {  // takes 2 and 3 are real voices
    tap(kRecX, kRecY);
    if (!until(Phase::Full, 600)) return fail(9, "recording did not fill the jar");
    frames(40);
  }
  tap(kRecX, kRecY);
  if (!until(Phase::Recording, 120) || game.take() != kWhisper) return fail(6, "fourth take must be the whisper");
  if (!until(Phase::TooQuiet, 300)) return fail(7, "whisper was not 'too quiet'");
  if (!until(Phase::Empty, 120)) return fail(8, "too-quiet did not return to the empty jar");
  frames(20);
  tap(20, 20);
  frames(4);
  if (game.phase() != Phase::Start || game.music_on()) return fail(17, "Home did not return to start");
  if (audio_peak <= 0) return fail(18, "no audio rendered after start");
  if (score.dropped()) return fail(19, "score command queue overflowed");
  if (!screenshot.empty() && !write_bmp(screenshot, screen.frame)) return fail(20, "cannot write screenshot");
  std::printf("host check OK: seed=%u frames=%d host render avg %.3f ms max %.3f ms; score peak %d; sprites=%zu "
              "pack=%u bytes; screenshot=%s\n",
              seed, screen.frames, screen.render_ms_total / screen.frames, screen.render_ms_max, audio_peak, pack.count(),
              pack.bytes(), screenshot.empty() ? "(none)" : screenshot.c_str());
  return 0;
}

int run_window(const std::string& pack_path, uint32_t seed, int zoom, bool mute, bool reduced, const std::string& script,
               double exit_after, const std::string& window_shot) {
  auto* display = lv_sdl_window_create(W, H);
  if (!display) { std::fputs("SDL LVGL window creation failed\n", stderr); return 1; }
  lv_sdl_window_set_zoom(display, static_cast<float>(zoom));
  lv_sdl_window_set_title(display, "Sound Jar (LVGL desktop)");
  lv_tick_set_cb(SDL_GetTicks);
  AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  DesktopAudio audio;
  if (!mute && !audio.open(4)) std::fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
  Game game(audio.score, seed);
  game.set_reduced(reduced);
  View view;
  const char* missing = nullptr;
  if (!view.create(lv_screen_active(), pack, &missing)) { std::fprintf(stderr, "Missing sprite %s\n", missing); return 1; }
  std::vector<Command> commands;
  if (!script.empty() && !parse_script(script, commands)) { std::fprintf(stderr, "Cannot read script %s\n", script.c_str()); return 1; }
  std::size_t next = 0;
  int pending_steps = 0;
  std::puts("Sound Jar: click/tap to play; hold a pad to loop it, slide up/down to bend. Home=top-left. "
            "Keys: R reduced motion, A again, H help, S start, Backspace home, Esc quit");
  bool pressed = false;
  uint32_t last = SDL_GetTicks();
  const uint32_t began = last;
  bool keys_prev[SDL_NUM_SCANCODES]{};
  int lmx = -1, lmy = -1;
  for (;;) {
    const uint32_t now = SDL_GetTicks();
    double dt = std::min(0.05, (now - last) / 1000.0);  // reference frame(): dt capped at 50 ms
    last = now;
    if (next < commands.size() || pending_steps) {  // scripted demo: deterministic 1/64 s steps
      dt = kStep;
      if (pending_steps) --pending_steps;
      else {
        const Command& c = commands[next++];
        if (c.op == "step") pending_steps = static_cast<int>(c.a);
        else apply(game, c);
        dt = 0;
      }
    }
    int mx = 0, my = 0;
    const bool down = (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    const double lx = static_cast<double>(mx) / zoom, ly = static_cast<double>(my) / zoom;
    if (down && !pressed) game.down(lx, ly);
    else if (down && (mx != lmx || my != lmy)) game.move(lx, ly);
    else if (!down && pressed) game.up();
    pressed = down; lmx = mx; lmy = my;
    if (dt > 0) game.tick(dt);
    view.sync(game);
    const uint32_t idle = lv_timer_handler();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    auto edge = [&](SDL_Scancode s) { return keys[s] && !keys_prev[s]; };
    if (keys[SDL_SCANCODE_ESCAPE]) break;
    if (edge(SDL_SCANCODE_R)) game.set_reduced(!game.reduced());
    if (edge(SDL_SCANCODE_A)) game.intent(Intent::AGAIN);
    if (edge(SDL_SCANCODE_H)) game.intent(Intent::HELP);
    if (edge(SDL_SCANCODE_S)) game.intent(Intent::START);
    if (edge(SDL_SCANCODE_BACKSPACE)) game.intent(Intent::BACK);
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) keys_prev[i] = keys[i] != 0;
    if (exit_after > 0 && (SDL_GetTicks() - began) / 1000.0 >= exit_after) break;
    SDL_Delay(std::min<uint32_t>(idle, 5));
  }
  if (!window_shot.empty()) {
    // The SDL driver renders in DIRECT mode into one full-screen RGB565 buffer, which it uploads to the window
    // texture as-is, so this is exactly the presented frame (OS capture fails when the desktop is locked).
    const lv_draw_buf_t* buf = lv_display_get_buf_active(display);
    if (!buf || buf->header.cf != LV_COLOR_FORMAT_RGB565) { std::fputs("window shot: unexpected buffer\n", stderr); return 1; }
    Frame f;
    for (int y = 0; y < H; ++y) std::memcpy(&f.pixels[y * W], buf->data + y * buf->header.stride, W * 2);
    if (!write_bmp(window_shot, f)) { std::fputs("window shot: cannot write\n", stderr); return 1; }
    std::printf("window shot: %s\n", window_shot.c_str());
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  int zoom = 2;
  bool check = false, mute = false, reduced = false, dump = false, verify = false, direct = false, window = false,
       profile = false;
  uint32_t seed = 1234;
  double exit_after = 0;
  std::string screenshot, script, pack;
  for (int i = 1; i < argc; ++i) {
    const std::string o = argv[i];
    const bool has = i + 1 < argc;
    if (o == "--check") check = true;
    else if (o == "--mute") mute = true;
    else if (o == "--reduced") reduced = true;
    else if (o == "--dump") dump = true;
    else if (o == "--verify-redraw") verify = true;
    else if (o == "--direct") direct = true;
    else if (o == "--window") window = true;
    else if (o == "--dirty-stats") g_dirty_stats = true;
    else if (o == "--profile") profile = true;
    else if (o == "--zoom" && has) zoom = std::atoi(argv[++i]);
    else if (o == "--seed" && has) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (o == "--screenshot" && has) screenshot = argv[++i];
    else if (o == "--script" && has) script = argv[++i];
    else if (o == "--pack" && has) pack = argv[++i];
    else if (o == "--exit-after" && has) exit_after = std::atof(argv[++i]);
    else {
      std::fprintf(stderr,
                   "Usage: sound_jar_native [--zoom 1..4] [--seed N] [--mute] [--reduced] [--pack FILE]\n"
                   "                        [--script FILE] [--exit-after S] [--window [--screenshot OUT.bmp]]\n"
                   "       sound_jar_native --check [--seed N] [--screenshot OUT.bmp]\n"
                   "       sound_jar_native --script FILE [--screenshot OUT.bmp] [--dump] [--verify-redraw] [--direct]\n"
                   "                        [--dirty-stats] [--profile]   (offscreen)\n");
      return 1;
    }
  }
  if (zoom < 1 || zoom > 4) { std::fputs("zoom must be 1..4\n", stderr); return 1; }
  const bool offscreen =
      !window && (check || (!script.empty() && (!screenshot.empty() || dump || verify || g_dirty_stats || profile)));
  if (SDL_Init(offscreen ? SDL_INIT_TIMER : SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  lv_init();
  if (check) return run_check(pack, seed, screenshot);
  if (offscreen) return run_script(script, pack, seed, reduced, screenshot, dump, verify, direct, profile);
  return run_window(pack, seed, zoom, mute, reduced, script, exit_after, screenshot);
}
