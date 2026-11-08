// Color Pop desktop build: LVGL 9 + SDL2. Mouse input is touch (pointerdown semantics, like the
// reference; Color Pop has no drag). Audio: ColorPopScore -> shared VolumeLimiter -> SDL.
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

#include "audio/color_pop_score.hpp"
#include "game/color_pop.hpp"
#include "little_arcade/volume.hpp"
#include "lvgl.h"
#include "ui/asset_pack.hpp"
#include "ui/color_pop_view.hpp"

namespace {
using namespace color_pop;
constexpr int W = kWidth, H = kHeight;
constexpr double kStep = 1.0 / 64;  // scripted/check step; the reference itself uses variable dt

// ---------------- audio ----------------
struct DesktopAudio {
  ColorPopScore score;
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
    want.freq = static_cast<int>(ColorPopScore::kSampleRate);
    want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 256;
    want.callback = callback; want.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!device) return false;
    limiter.set_level(level);  // limiter starts muted and ramps; the score is silent until the first tap
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
    path = std::string(base ? base : "") + "color_pop.pack";
    if (base) SDL_free(base);
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) { std::fprintf(stderr, "Cannot open asset pack %s (build the color_pop_assets target)\n", path.c_str()); return false; }
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
int g_hide_first = -1, g_hide_last = -1;
uint32_t sim_clock() { return g_sim_ms; }
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) {
  g_flushed += static_cast<uint64_t>(lv_area_get_width(area)) * lv_area_get_height(area);
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
// One command per line: `step N` (N frames of 1/64 s), `tap X Y`, `target` (tap the first
// floating target bubble), `wrong` (tap the first floating non-target bubble), `home`,
// `reduced 0|1`. '#' starts a comment.
struct Command { std::string op; double a = 0, b = 0; };
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
    ss >> c.a >> c.b;
    out.push_back(c);
  }
  return true;
}
const Bubble* floating(const Game& g, bool target) {
  for (const Bubble& b : g.bubbles())
    if ((b.color == g.target()) == target && b.state == BubbleState::Float) return &b;
  return nullptr;
}
// Applies a non-step command; returns false for an unknown/unsatisfiable command.
bool apply(Game& g, const Command& c) {
  if (c.op == "target?") {  // optional tap (bench scripts): skipped when no target is floating
    if (const Bubble* b = floating(g, true)) g.down(b->x, b->y);
    return true;
  }
  if (c.op == "tap") { g.down(c.a, c.b); return true; }
  if (c.op == "home") { g.down(20, 20); return true; }
  if (c.op == "reduced") { g.set_reduced(c.a != 0); return true; }
  if (c.op == "target" || c.op == "wrong") {
    const Bubble* b = floating(g, c.op == "target");
    if (!b) { std::fprintf(stderr, "script: no floating %s bubble\n", c.op.c_str()); return false; }
    g.down(b->x, b->y);
    return true;
  }
  std::fprintf(stderr, "script: unknown command '%s'\n", c.op.c_str());
  return false;
}

const char* phase_name(Phase p) {
  switch (p) {
    case Phase::Start: return "START";
    case Phase::Intro: return "INTRO";
    case Phase::Play: return "PLAY";
    case Phase::Celebrate: return "CELEBRATE";
  }
  return "?";
}
void dump_state(const Game& g) {
  static const char* kColor[4] = {"RED", "YELLOW", "GREEN", "BLUE"};
  std::printf("{\"phase\":\"%s\",\"round\":%d,\"target\":\"%s\",\"count\":%d,\"hint\":%d,\"successes\":%d,"
              "\"parts\":%zu,\"decor\":%zu,\"rings\":%zu,\"bubbles\":[",
              phase_name(g.phase()), g.round(), g.has_target() ? kColor[static_cast<int>(g.target())] : "",
              g.count(), g.hint(), g.successes(), g.particles().size(), g.decor().size(), g.rings().size());
  for (std::size_t i = 0; i < g.bubbles().size(); ++i) {
    const Bubble& b = g.bubbles()[i];
    std::printf("%s{\"color\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"s\":%.4f,\"sx\":%.4f,\"sy\":%.4f,\"rot\":%.4f}", i ? "," : "",
                kColor[static_cast<int>(b.color)], b.x, b.y, b.s, b.sx, b.sy, b.rot);
  }
  std::printf("]}\n");
}

struct Offscreen {
  Frame frame;
  lv_display_t* display = nullptr;
  double clock = 0;
  double render_ms_total = 0, render_ms_max = 0;
  int frames = 0;
  bool verify = false;           // --verify-redraw: compare each incremental frame with a full redraw
  long long stale_pixels = 0;
  int stale_frames = 0, max_delta = 0;
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
  int batch = 1;                 // --batch N: sync N simulation steps per render (window-loop emulation)
  int pending = 0;
  std::vector<uint32_t> dirty;   // per rendered frame: redrawn pixels
  int hide_first = -1, hide_last = -1;  // --dirty-stats A B: keep screen children A..B hidden
  void step(Game& g, View& v) {
    g.tick(kStep);
    clock += kStep;
    g_sim_ms = static_cast<uint32_t>(clock * 1000);
    v.sync(g);
    for (int i = hide_first; i >= 0 && i <= hide_last; ++i)
      lv_obj_add_flag(lv_obj_get_child(lv_screen_active(), i), LV_OBJ_FLAG_HIDDEN);
    if (++pending < batch) return;
    pending = 0;
    const uint64_t flushed = g_flushed;
    const auto t0 = std::chrono::steady_clock::now();
    lv_refr_now(display);
    dirty.push_back(static_cast<uint32_t>(g_flushed - flushed));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    render_ms_total += ms; render_ms_max = std::max(render_ms_max, ms); ++frames;
    if (verify) {
      const auto incremental = frame.pixels;
      lv_obj_invalidate(lv_screen_active());
      lv_refr_now(display);
      long long diff = 0;
      int x0 = W, y0 = H, x1 = -1, y1 = -1, worst = 0;
      for (std::size_t i = 0; i < incremental.size(); ++i)
        if (incremental[i] != frame.pixels[i]) {
          const uint16_t a = incremental[i], b = frame.pixels[i];
          // RGB565 channel deltas scaled to 8 bits. Transformed (scaled/rotated) images sample
          // differently when split across partial-buffer slices: allow small rounding noise.
          const int d = std::max({std::abs((a >> 11) - (b >> 11)) * 8, std::abs(((a >> 5) & 63) - ((b >> 5) & 63)) * 4,
                                  std::abs((a & 31) - (b & 31)) * 8});
          worst = std::max(worst, d);
          if (d <= 48) continue;
          ++diff;
          const int x = static_cast<int>(i % W), y = static_cast<int>(i / W);
          x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
        }
      max_delta = std::max(max_delta, worst);
      if (diff) {
        if (stale_frames < 8)
          std::fprintf(stderr, "redraw mismatch at frame %d: %lld px in [%d,%d..%d,%d]\n", frames, diff, x0, y0, x1, y1);
        stale_pixels += diff; ++stale_frames;
      }
    }
  }
};

int run_script(const std::string& script, const std::string& pack_path, uint32_t seed, bool reduced,
               const std::string& screenshot, bool dump, bool verify, bool direct, int batch) {
  std::vector<Command> commands;
  if (!parse_script(script, commands)) { std::fprintf(stderr, "Cannot read script %s\n", script.c_str()); return 1; }
  AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  Offscreen screen(direct);
  screen.verify = verify;
  screen.batch = std::max(1, batch);
  screen.hide_first = g_hide_first;
  screen.hide_last = g_hide_last;
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
  if (!screenshot.empty() && !write_bmp(screenshot, screen.frame)) { std::fprintf(stderr, "Cannot write %s\n", screenshot.c_str()); return 3; }
  if (dump) dump_state(game);
  if (g_dirty_stats && !screen.dirty.empty()) {
    std::vector<uint32_t> d = screen.dirty;
    std::sort(d.begin(), d.end());
    auto pct = [&](int p) { return 100.0 * d[std::min(d.size() - 1, d.size() * p / 100)] / (W * H); };
    const auto full = std::count_if(d.begin(), d.end(), [](uint32_t v) { return v * 2 > W * H; });
    std::printf("dirty frames=%zu p50=%.1f%% p95=%.1f%% max=%.1f%% over50%%=%lld\n", d.size(), pct(50), pct(95), pct(100),
                static_cast<long long>(full));
  }
  if (verify) {
    std::fprintf(stderr, "redraw verification: %d/%d frames differ, %lld stale px (max channel delta %d)\n",
                 screen.stale_frames, screen.frames, screen.stale_pixels, screen.max_delta);
    if (screen.stale_frames) return 4;
  }
  return 0;
}

// Built-in offscreen interaction/render check (ctest).
int run_check(const std::string& pack_path, uint32_t seed, const std::string& screenshot) {
  AssetPack pack;
  if (!load_pack(pack, pack_path)) return 1;
  Offscreen screen;
  ColorPopScore score;  // real score, rendered offline to prove the event path is sound
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
  auto fail = [&](int code, const char* why) { std::fprintf(stderr, "check failed: %s\n", why); dump_state(game); return code; };
  frames(10);
  if (game.phase() != Phase::Start || audio_peak != 0) return fail(2, "start screen must be silent");
  game.down(160, 120);
  frames(140);
  if (game.phase() != Phase::Play) return fail(3, "intro did not reach play");
  const Bubble* w = floating(game, false);
  if (!w) return fail(4, "no distractor");
  game.down(w->x, w->y);
  frames(20);
  if (game.misses() != 1 || game.hint() != 1 || game.phase() != Phase::Play) return fail(5, "wrong tap not forgiving");
  const Bubble* t = floating(game, true);
  if (!t) return fail(6, "no target");
  game.down(t->x, t->y);
  frames(20);
  if (game.phase() != Phase::Celebrate || game.particles().empty()) return fail(7, "target pop did not celebrate");
  for (int i = 0; i < 30; ++i) { game.down(40 + i * 8, 80 + (i * 13) % 120); frames(1); }  // mash
  if (static_cast<int>(game.particles().size()) > kMaxParticles) return fail(8, "particle cap exceeded");
  frames(110);
  if (game.phase() != Phase::Play || game.round() != 2) return fail(9, "next round did not start");
  game.down(20, 20);
  frames(4);
  if (game.phase() != Phase::Start) return fail(10, "Home did not return to start");
  if (audio_peak <= 0) return fail(11, "no audio rendered after start");
  if (!screenshot.empty() && !write_bmp(screenshot, screen.frame)) return fail(12, "cannot write screenshot");
  std::printf("host check OK: seed=%u rounds=%d frames=%d host render avg %.3f ms max %.3f ms; score peak %d; "
              "sprites=%zu pack=%u bytes; screenshot=%s\n",
              seed, game.round(), screen.frames, screen.render_ms_total / screen.frames, screen.render_ms_max,
              audio_peak, pack.count(), pack.bytes(), screenshot.empty() ? "(none)" : screenshot.c_str());
  return 0;
}

int run_window(const std::string& pack_path, uint32_t seed, int zoom, bool mute, bool reduced, const std::string& script,
               double exit_after) {
  auto* display = lv_sdl_window_create(W, H);
  if (!display) { std::fputs("SDL LVGL window creation failed\n", stderr); return 1; }
  lv_sdl_window_set_zoom(display, static_cast<float>(zoom));
  lv_sdl_window_set_title(display, "Color Pop (LVGL desktop)");
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
  std::puts("Color Pop: click/tap to play. Home=top-left. Keys: R reduced motion, 1-4 red/blue/green/yellow, "
            "A again, H help, S start, Backspace home, Esc quit");
  bool pressed = false;
  uint32_t last = SDL_GetTicks();
  const uint32_t began = last;
  bool keys_prev[SDL_NUM_SCANCODES]{};
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
    if (down && !pressed) game.down(static_cast<double>(mx) / zoom, static_cast<double>(my) / zoom);
    pressed = down;
    if (dt > 0) game.tick(dt);
    view.sync(game);
    const uint32_t idle = lv_timer_handler();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    auto edge = [&](SDL_Scancode s) { const bool hit = keys[s] && !keys_prev[s]; return hit; };
    if (keys[SDL_SCANCODE_ESCAPE]) break;
    using little_arcade::Intent;
    if (edge(SDL_SCANCODE_R)) game.set_reduced(!game.reduced());
    if (edge(SDL_SCANCODE_1)) game.intent(Intent::RED);
    if (edge(SDL_SCANCODE_2)) game.intent(Intent::BLUE);
    if (edge(SDL_SCANCODE_3)) game.intent(Intent::GREEN);
    if (edge(SDL_SCANCODE_4)) game.intent(Intent::YELLOW);
    if (edge(SDL_SCANCODE_A)) game.intent(Intent::AGAIN);
    if (edge(SDL_SCANCODE_H)) game.intent(Intent::HELP);
    if (edge(SDL_SCANCODE_S)) game.intent(Intent::START);
    if (edge(SDL_SCANCODE_BACKSPACE)) game.intent(Intent::BACK);
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) keys_prev[i] = keys[i] != 0;
    if (exit_after > 0 && (SDL_GetTicks() - began) / 1000.0 >= exit_after) break;
    SDL_Delay(std::min<uint32_t>(idle, 5));
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  int zoom = 2, batch = 1;
  bool check = false, mute = false, reduced = false, dump = false, verify = false, direct = false;
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
    else if (o == "--batch" && has) batch = std::atoi(argv[++i]);
    else if (o == "--zoom" && has) zoom = std::atoi(argv[++i]);
    else if (o == "--seed" && has) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    else if (o == "--screenshot" && has) screenshot = argv[++i];
    else if (o == "--script" && has) script = argv[++i];
    else if (o == "--pack" && has) pack = argv[++i];
    else if (o == "--exit-after" && has) exit_after = std::atof(argv[++i]);
    else if (o == "--dirty-stats") {
      g_dirty_stats = true;
      if (i + 2 < argc && argv[i + 1][0] != '-') { g_hide_first = std::atoi(argv[++i]); g_hide_last = std::atoi(argv[++i]); }
    }
    else {
      std::fprintf(stderr,
                   "Usage: color_pop_native [--zoom 1..4] [--seed N] [--mute] [--reduced] [--pack FILE]\n"
                   "                        [--script FILE] [--exit-after S]   (window; script = scripted demo)\n"
                   "       color_pop_native --check [--seed N] [--screenshot OUT.bmp]\n"
                   "       color_pop_native --script FILE [--screenshot OUT.bmp] [--dump] [--verify-redraw] [--direct] [--batch N]\n"
                   "                        [--dirty-stats [FIRST LAST]]   (offscreen; FIRST..LAST = screen children to hide)\n");
      return 1;
    }
  }
  if (zoom < 1 || zoom > 4) { std::fputs("zoom must be 1..4\n", stderr); return 1; }
  const bool offscreen = check || (!script.empty() && (!screenshot.empty() || dump || verify || g_dirty_stats));
  if (SDL_Init(offscreen ? SDL_INIT_TIMER : SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  lv_init();
  if (check) return run_check(pack, seed, screenshot);
  if (offscreen) return run_script(script, pack, seed, reduced, screenshot, dump, verify, direct, batch);
  return run_window(pack, seed, zoom, mute, reduced, script, exit_after);
}
