// Sound Jar ESP32-S3 target: the portable game (src/game), the LVGL 9 sprite view (src/ui) and the ported
// Web Audio score (src/audio) on the Little Arcade board components. The sprite pack written by
// tools/export_assets.mjs is flashed to the `assets` partition, verified against this build's signature and
// copied once into PSRAM (measured: drawing straight from the flash mapping was ~7 fps); every lv_image_dsc_t
// points into that copy. The built-in takes (PCM) live in app rodata.
// The INMP441 microphone (owner-reported wiring, config/board_pins.json) records the child's take when the parent
// mic switch (settings_store, default on) allows it: one <= 3 s clip in PSRAM, never stored, logged or sent, and
// wiped on Home, the bin, too-quiet and every new recording. Without a mic the built-in voices play (SJ-5).
#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#include "audio/sound_jar_score.hpp"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game/live_take.hpp"
#include "game/sound_jar.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/i2s_audio.hpp"
#include "little_arcade/audio/mic_sink.hpp"
#include "little_arcade/board_io.hpp"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/settings/settings_store.hpp"
#include "lvgl.h"
#include "pack_signature.hpp"
#include "ui/asset_pack.hpp"
#include "ui/render_profile.hpp"
#include "ui/sound_jar_view.hpp"

namespace {
constexpr const char* tag = "sound_jar";
constexpr int screen_w = 320, screen_h = 240, buffer_lines = 24;
constexpr size_t buffer_bytes = screen_w * buffer_lines * 2;
constexpr uint32_t refresh_ms = 20;    // frame cadence (50 FPS target; PF-1 needs >= 30 FPS p95)
constexpr uint32_t touch_poll_ms = 5;  // NS2009 poll: two 100 kHz I2C transactions, ~0.7 ms
esp_lcd_panel_handle_t panel = nullptr;
uint64_t frame_pixels = 0;
uint32_t flush_calls = 0;
uint64_t flush_block_us = 0;  // time the UI task waits inside draw_bitmap (SPI back-pressure)

uint32_t clock_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void flush_done(void* ctx) { lv_display_flush_ready(static_cast<lv_display_t*>(ctx)); }
void flush(lv_display_t*, const lv_area_t* area, uint8_t* pixels) {
  const uint32_t count = static_cast<uint32_t>(lv_area_get_width(area) * lv_area_get_height(area));
  lv_draw_sw_rgb565_swap(pixels, count);
  frame_pixels += count;
  ++flush_calls;
  const int64_t t0 = esp_timer_get_time();
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
  flush_block_us += static_cast<uint64_t>(esp_timer_get_time() - t0);
}
void memory_log(const char* label) {
  ESP_LOGI(tag, "[mem:%s] internal=%u min=%u psram=%u min=%u", label,
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}

// Maps the asset partition after checking that it holds the pack this firmware was built with.
const uint8_t* map_pack(size_t* size) {
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets");
  if (!part) { ESP_LOGE(tag, "no 'assets' partition"); return nullptr; }
  uint8_t header[32];
  if (esp_partition_read(part, 0, header, sizeof(header)) != ESP_OK) { ESP_LOGE(tag, "assets unreadable"); return nullptr; }
  uint32_t bytes = 0;
  std::memcpy(&bytes, header + 12, 4);
  if (std::memcmp(header, "SJAR", 4) != 0 || bytes != sound_jar::pack::bytes ||
      std::memcmp(header + 16, sound_jar::pack::signature, 16) != 0 || bytes > part->size) {
    ESP_LOGE(tag, "assets partition does not hold this build's sound_jar.pack (expected %u bytes); flash it with flash.ps1",
             static_cast<unsigned>(sound_jar::pack::bytes));
    return nullptr;
  }
  const void* ptr = nullptr;
  esp_partition_mmap_handle_t handle;
  if (esp_partition_mmap(part, 0, bytes, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK) {
    ESP_LOGE(tag, "assets mmap failed");
    return nullptr;
  }
  *size = bytes;
#if !SOUND_JAR_PACK_IN_PSRAM
  // Measured on the board: copying the pack into octal PSRAM (149 ms at boot, -2.3 MB PSRAM) left the frame
  // time unchanged (~7 fps on START either way): rendering is CPU-bound, so draw straight from the mapping.
  return static_cast<const uint8_t*>(ptr);
#else
  auto* copy = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
  if (!copy) {
    ESP_LOGW(tag, "no PSRAM for the pack; drawing from flash");
    return static_cast<const uint8_t*>(ptr);
  }
  const int64_t t0 = esp_timer_get_time();
  std::memcpy(copy, ptr, bytes);
  esp_partition_munmap(handle);
  ESP_LOGI(tag, "pack copied to PSRAM in %u ms", static_cast<unsigned>((esp_timer_get_time() - t0) / 1000));
  return copy;
#endif
}

const char* phase_name(sound_jar::Phase p) {
  switch (p) {
    case sound_jar::Phase::Start: return "START";
    case sound_jar::Phase::Intro: return "INTRO";
    case sound_jar::Phase::Empty: return "EMPTY";
    case sound_jar::Phase::Countdown: return "COUNTDOWN";
    case sound_jar::Phase::Recording: return "RECORDING";
    case sound_jar::Phase::Processing: return "PROCESSING";
    case sound_jar::Phase::Full: return "FULL";
    case sound_jar::Phase::TooQuiet: return "TOO_QUIET";
  }
  return "?";
}

// Measures the score's share of the audio task (PF-9: audio <= 25 % of one core).
class TimedScore final : public little_arcade::audio::ExternalScore {
 public:
  explicit TimedScore(little_arcade::audio::ExternalScore& inner) : inner_(inner) {}
  void render_add(int32_t* out, std::size_t n) override {
    const int64_t t0 = esp_timer_get_time();
    inner_.render_add(out, n);
    const uint32_t us = static_cast<uint32_t>(esp_timer_get_time() - t0);
    us_.fetch_add(us, std::memory_order_relaxed);
    samples_.fetch_add(static_cast<uint32_t>(n), std::memory_order_relaxed);
    uint32_t prev = max_us_.load(std::memory_order_relaxed);
    while (us > prev && !max_us_.compare_exchange_weak(prev, us, std::memory_order_relaxed)) {}
  }
  // Percent of one core spent rendering the score since the last call, and the worst block.
  void take(double* load_pct, uint32_t* max_us, uint32_t* samples) {
    const uint32_t us = us_.exchange(0), n = samples_.exchange(0);
    *max_us = max_us_.exchange(0);
    *samples = n;
    *load_pct = n ? 100.0 * us / (n * 1e6 / sound_jar::SoundJarScore::kSampleRate) : 0;
  }

 private:
  little_arcade::audio::ExternalScore& inner_;
  std::atomic<uint32_t> us_{0}, samples_{0}, max_us_{0};
};

// Hands INMP441 blocks from the shared I2S driver to the game's LiveTake and times them (audio task share).
class MicAdapter final : public little_arcade::audio::MicSink {
 public:
  explicit MicAdapter(sound_jar::LiveTake& take) : take_(take) {}
  void set_mic_hardware(bool present) override { take_.set_hardware(present); }
  void capture(const int16_t* samples, std::size_t n) override {
    const int64_t t0 = esp_timer_get_time();
    take_.capture(samples, n);
    const uint32_t us = static_cast<uint32_t>(esp_timer_get_time() - t0);
    us_.fetch_add(us, std::memory_order_relaxed);
    blocks_.fetch_add(1, std::memory_order_relaxed);
    uint32_t prev = max_us_.load(std::memory_order_relaxed);
    while (us > prev && !max_us_.compare_exchange_weak(prev, us, std::memory_order_relaxed)) {}
  }
  void take(uint32_t* total_us, uint32_t* max_us, uint32_t* blocks) {
    *total_us = us_.exchange(0);
    *max_us = max_us_.exchange(0);
    *blocks = blocks_.exchange(0);
  }

 private:
  sound_jar::LiveTake& take_;
  std::atomic<uint32_t> us_{0}, max_us_{0}, blocks_{0};
};
uint64_t clock_us() { return static_cast<uint64_t>(esp_timer_get_time()); }

// Per-window frame statistics: render time (lv_refr_now incl. waiting for SPI DMA) and the interval between
// rendered frames, both p95, plus the dirty fraction.
struct FrameStats {
  static constexpr size_t kMax = 1024;
  uint32_t render_us[kMax], interval_us[kMax], dirty_px[kMax];
  size_t n = 0, ni = 0;
  uint32_t max_render_us = 0, max_dirty = 0, loops = 0;
  uint64_t touch_us = 0, tick_us = 0, sync_us = 0, flushes = 0, block_us = 0;
  int64_t last_frame_us = 0;
  void add(uint32_t render, uint32_t pixels, uint32_t calls, uint64_t blocked, int64_t now_us) {
    if (n < kMax) { render_us[n] = render; dirty_px[n] = pixels; ++n; }
    flushes += calls;
    block_us += blocked;
    max_render_us = std::max(max_render_us, render);
    max_dirty = std::max(max_dirty, pixels);
    if (last_frame_us && ni < kMax) interval_us[ni++] = static_cast<uint32_t>(now_us - last_frame_us);
    last_frame_us = now_us;
  }
  static uint32_t pct(uint32_t* v, size_t count, int p) {
    if (!count) return 0;
    const size_t k = std::min(count - 1, (count * p) / 100);
    std::nth_element(v, v + k, v + count);
    return v[k];
  }
  void report(const sound_jar::Game& game, sound_jar::SoundJarScore& score, TimedScore& timed) {
    const uint32_t r50 = pct(render_us, n, 50), r95 = pct(render_us, n, 95);
    const uint32_t i50 = pct(interval_us, ni, 50), i95 = pct(interval_us, ni, 95);
    const uint32_t d50 = pct(dirty_px, n, 50), d95 = pct(dirty_px, n, 95);
    const unsigned l = loops ? loops : 1, f = n ? static_cast<unsigned>(n) : 1;
    constexpr uint32_t screen = screen_w * screen_h;
    double audio_pct = 0;
    uint32_t audio_max_us = 0, audio_samples = 0;
    timed.take(&audio_pct, &audio_max_us, &audio_samples);
    ESP_LOGI(tag,
             "[frames] n=%u render p50=%u p95=%u max=%u us | interval p50=%u p95=%u us (p95 %.1f fps) | dirty p50=%u%% "
             "p95=%u%% max=%u%% flushes/frame=%u spi_wait/frame=%u us | loop avg us: touch=%u tick=%u sync=%u",
             static_cast<unsigned>(n), static_cast<unsigned>(r50), static_cast<unsigned>(r95),
             static_cast<unsigned>(max_render_us), static_cast<unsigned>(i50), static_cast<unsigned>(i95),
             i95 ? 1e6 / i95 : 0.0, static_cast<unsigned>(d50 * 100 / screen), static_cast<unsigned>(d95 * 100 / screen),
             static_cast<unsigned>(max_dirty * 100 / screen), static_cast<unsigned>(flushes / f),
             static_cast<unsigned>(block_us / f), static_cast<unsigned>(touch_us / l),
             static_cast<unsigned>(tick_us / l), static_cast<unsigned>(sync_us / l));
    ESP_LOGI(tag,
             "[game] phase=%s take=%d tries=%d actions=%d voices=%u parts=%u rings=%u fx=%u | [audio] score load=%.1f%% "
             "of one core, worst block=%u us, samples=%u, score_drops=%u | stack_free=%u",
             phase_name(game.phase()), game.take(), game.tries(), game.actions(),
             static_cast<unsigned>(game.voices().size()), static_cast<unsigned>(game.particles().size()),
             static_cast<unsigned>(game.rings().size()), static_cast<unsigned>(game.fx().size()), audio_pct,
             static_cast<unsigned>(audio_max_us), static_cast<unsigned>(audio_samples),
             static_cast<unsigned>(score.dropped()), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    n = ni = 0;
    max_render_us = max_dirty = loops = 0;
    touch_us = tick_us = sync_us = flushes = block_us = 0;
  }
};

#ifdef SOUND_JAR_BENCH
// Plays like a helpful child: taps the reference autoTarget() and, every third pad, holds it to loop and slides
// up to bend, so recording, pouring, effects, loops and particles all appear in the frame statistics.
struct Bench {
  uint32_t next = 0, pressed_at = 0, release = 0;
  int taps = 0;
  bool down = false, holding = false;
  double x = 0, y = 0;
  void step(sound_jar::Game& game, uint32_t now) {
    if (down) {
      if (holding) game.move(x, y - std::min(40.0, (now - pressed_at) * 0.05));  // slide up 40 px over 0.8 s
      if (now >= release) { game.up(); down = holding = false; next = now + 500; }
      return;
    }
    if (now < next) return;
    if (game.phase() == sound_jar::Phase::Start) { x = 160; y = 120; }
    else if (!game.auto_target(x, y)) { next = now + 300; return; }
    ++taps;
    holding = y >= 168 && taps % 3 == 0;
    game.down(x, y);
    down = true;
    pressed_at = now;
    release = now + (holding ? 1400 : 120);
  }
};
#endif

little_arcade::VolumeLimiter::Config speaker_config() {
  little_arcade::VolumeLimiter::Config cfg;
  cfg.sample_rate_hz = little_arcade::audio::kSampleRateHz;
  cfg.peak_limit = little_arcade::VolumeLimiter::amp_safe_peak(little_arcade::board::audio::kAmpGainDb,
                                                               little_arcade::board::audio::kAmpSupplyMv);
  return cfg;
}

void run(void*) {
  uint32_t flash_size = 0;
  ESP_ERROR_CHECK(esp_flash_get_size(nullptr, &flash_size));
  ESP_LOGI(tag, "flash=%" PRIu32 " psram=%u pack=%u bytes, %u sprites%s", flash_size,
           static_cast<unsigned>(esp_psram_is_initialized() ? esp_psram_get_size() : 0),
           static_cast<unsigned>(sound_jar::pack::bytes), static_cast<unsigned>(sound_jar::pack::entries),
#ifdef SOUND_JAR_BENCH
           " [BENCH: auto-plays autoTarget()]"
#else
           ""
#endif
  );
  memory_log("boot");
  size_t pack_size = 0;
  const uint8_t* pack_data = map_pack(&pack_size);
  static sound_jar::AssetPack pack;
  std::string error;
  if (!pack_data || !pack.load(pack_data, pack_size, &error)) {
    ESP_LOGE(tag, "sprite pack unusable %s; refusing to start", error.c_str());
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(tag, "pack mapped at %p: %u sprites", pack_data, static_cast<unsigned>(pack.count()));

  lv_init();
  lv_tick_set_cb(clock_ms);
  auto* display = lv_display_create(screen_w, screen_h);
  little_arcade::board::DisplayConfig cfg;
  cfg.pclk_hz = 40 * 1000 * 1000;
  cfg.max_transfer_bytes = buffer_bytes;
  cfg.on_flush_done = flush_done;
  cfg.ctx = display;
  ESP_ERROR_CHECK(little_arcade::board::display_init(cfg, &panel));
  void* first = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* second = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  configASSERT(first && second);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, first, second, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  // Rendering is driven explicitly with lv_refr_now() once per frame (see the loop), so keep LVGL's own refresh
  // timer from firing between frames with half-synced state.
  lv_timer_set_period(lv_display_get_refr_timer(display), 3600u * 1000u);
  const esp_err_t touch = little_arcade::board::touch_init();
  if (touch != ESP_OK) ESP_LOGE(tag, "touch unavailable: %s", esp_err_to_name(touch));

  // Audio objects first so the score exists before the game can call it; sound starts silent (master ramps
  // 0 -> 0.35 like initAudio) and only after the child taps Start.
  static sound_jar::SoundJarScore score;
  static TimedScore timed(score);
  static little_arcade::audio::AudioFeedback feedback(speaker_config());
  feedback.attach_score(&timed);
  static sound_jar::Game game(score, 1234);  // reference default (?seed absent)
  // Microphone take: a 96 KB clip buffer in PSRAM (zeroed here and on every wipe).
  static sound_jar::LiveTake* mic = nullptr;
  static MicAdapter* mic_adapter = nullptr;
  if (auto* clip = static_cast<int16_t*>(
          heap_caps_malloc(sizeof(int16_t) * sound_jar::LiveTake::kMaxSamples, MALLOC_CAP_SPIRAM))) {
    mic = new (std::nothrow) sound_jar::LiveTake(clip, clock_us);
    // INMP441: -26 dBFS at 94 dB SPL, so speech at 30-50 cm is near -58 dBFS. x16 (+24 dB) lifts it past the
    // reference's LOUD (-36 dBFS, tuned on browser AGC input); the mic noise floor (-87 dBFS) stays well below ACTIVE.
    if (mic) mic->set_input_gain(16.f);
    if (mic) mic_adapter = new (std::nothrow) MicAdapter(*mic);
  }
  if (!mic || !mic_adapter) ESP_LOGE(tag, "no PSRAM for the mic clip; built-in voices only");
#ifndef SOUND_JAR_BENCH  // the bench keeps the deterministic built-in takes so frame windows stay comparable
  if (mic && mic_adapter) {
    game.set_mic(mic);
    score.set_live(mic);
  }
#endif
  memory_log("before_view");
  static sound_jar::View view;
  const char* missing = nullptr;
  if (!view.create(lv_screen_active(), pack, &missing)) {
    ESP_LOGE(tag, "sprite %s missing from pack; refusing to start", missing ? missing : "?");
    vTaskDelete(nullptr);
    return;
  }
  view.sync(game);
  lv_refr_now(display);
  memory_log("after_first_frame");
  little_arcade::board::backlight_set(true);

  little_arcade::settings::init_store();
  const uint8_t volume = little_arcade::settings::load_volume_level();
  feedback.set_level(volume);
  little_arcade::audio::I2sAudioOptions options;
  options.mic_level_probe = false;  // the top-right voice-command mic stays disabled; this is the record mic only
  const bool mic_enabled = little_arcade::settings::load_mic_enabled();
  if (mic) mic->set_allowed(mic_enabled);
#ifndef SOUND_JAR_BENCH
  if (mic_adapter && mic_enabled) options.mic_sink = mic_adapter;
#endif
  if (!little_arcade::audio::start_i2s_audio(feedback, options))
    ESP_LOGE(tag, "audio unavailable; the game stays fully playable by touch with visual cues");
  ESP_LOGI(tag, "saved parent volume=%u (0 = muted); mic switch=%s hardware=%s -> recordings use %s; ready", volume,
           mic_enabled ? "on" : "off", mic && mic->hardware() ? "yes" : "no",
           game.mic_live() ? "the microphone" : "built-in voices");
  memory_log("audio_started");

  static FrameStats stats;
  bool pressed = false;
  int last_x = -1, last_y = -1;
  uint32_t last = clock_ms(), report = last, last_frame = 0, last_touch = 0;
#ifdef SOUND_JAR_BENCH
  static Bench bench;
  bench.next = last + 1500;
#endif
  for (;;) {
    const uint32_t now = clock_ms();
    const int64_t loop_t0 = esp_timer_get_time();
    if (touch == ESP_OK && now - last_touch >= touch_poll_ms) {
      last_touch = now;
      little_arcade::board::TouchSample sample{};
      if (little_arcade::board::touch_read(&sample) == ESP_OK && sample.pressed) {
        const auto point = little_arcade::board::ns2009_to_screen(sample.raw_x, sample.raw_y);
        // pointerdown, then pointermove while held (pad loop + pitch bend), then pointerup.
        if (!pressed) game.down(point.x, point.y);
        else if (point.x != last_x || point.y != last_y) game.move(point.x, point.y);
        last_x = point.x; last_y = point.y;
        pressed = true;
      } else if (pressed) {
        game.up();
        pressed = false;
      }
    }
#ifdef SOUND_JAR_BENCH
    bench.step(game, now);
#endif
    const int64_t touch_t1 = esp_timer_get_time();
    // Tick, sync and render once per frame; touch is polled every iteration in between.
    if (now - last_frame < refresh_ms) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
    last_frame = now;
    // Game time follows the wall clock: at ~10 fps a single 50 ms-capped tick ran every animation at half speed.
    const double elapsed = (now - last) / 1000.0;
    last = now;
    if (elapsed > 0) game.advance(elapsed);
    const int64_t tick_t1 = esp_timer_get_time();
    view.sync(game);
    const uint64_t before = frame_pixels, block_before = flush_block_us;
    const uint32_t calls_before = flush_calls;
    const int64_t t0 = esp_timer_get_time();
    stats.touch_us += static_cast<uint64_t>(touch_t1 - loop_t0);
    stats.tick_us += static_cast<uint64_t>(tick_t1 - touch_t1);
    stats.sync_us += static_cast<uint64_t>(t0 - tick_t1);
    ++stats.loops;
    lv_refr_now(display);
    if (frame_pixels > before) {
      const int64_t t1 = esp_timer_get_time();
      stats.add(static_cast<uint32_t>(t1 - t0), static_cast<uint32_t>(frame_pixels - before), flush_calls - calls_before,
                flush_block_us - block_before, t1);
    }
    if (now - report >= 10000) {
      stats.report(game, score, timed);
      if (mic_adapter && mic) {
        uint32_t mic_us = 0, mic_max = 0, mic_blocks = 0;
        mic_adapter->take(&mic_us, &mic_max, &mic_blocks);
        // Aggregate diagnostics only (one loudest-frame level per recording, for gain calibration); no samples.
        const double peak = mic->peak_level(), floor = mic->floor_level();
        auto dbfs = [](double v) { return v > 0 ? static_cast<int>(std::lround(20 * std::log10(v))) : -120; };
        ESP_LOGI(tag, "[mic] live=%d state=%u takes=%u jar_live=%d start_latency=%u us last_peak=%d last_floor=%d dBFS | capture blocks=%u avg=%u max=%u us",
                 game.mic_live() ? 1 : 0, static_cast<unsigned>(mic->state()), static_cast<unsigned>(mic->takes()),
                 game.live_clip() ? 1 : 0, static_cast<unsigned>(mic->start_latency_us()),
                 dbfs(peak), dbfs(floor),
                 static_cast<unsigned>(mic_blocks), static_cast<unsigned>(mic_blocks ? mic_us / mic_blocks : 0),
                 static_cast<unsigned>(mic_max));
      }
      memory_log("playing");
#ifdef SOUND_JAR_BENCH
      // Once, mid-play: full-screen render cost attributed to each visible layer (stalls ~15 s; the next
      // [frames] window includes that stall).
      static bool profiled = false;
      if (!profiled && game.phase() == sound_jar::Phase::Full) {
        profiled = true;
        std::vector<sound_jar::LayerCost> costs;
        const uint32_t base = sound_jar::profile_layers(
            display, [] { return static_cast<uint64_t>(esp_timer_get_time()); }, costs, 1);
        static char line[1600];
        sound_jar::format_profile(line, sizeof line, base, costs, 24);
        ESP_LOGI(tag, "[profile] %s", line);
      }
#endif
      report = clock_ms();
    }
    lv_timer_handler();  // housekeeping only; rendering is driven by lv_refr_now above
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
}  // namespace

extern "C" void app_main() {
  BaseType_t created = xTaskCreatePinnedToCore(run, "sound_jar_ui", 24576, nullptr, 4, nullptr, 1);
  configASSERT(created == pdPASS);
}
