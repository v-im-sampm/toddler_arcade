// Color Pop ESP32-S3 target: the portable game (src/game), the LVGL 9 sprite view (src/ui) and
// the ported Web Audio score (src/audio) on the Little Arcade board components. The sprite pack
// written by tools/export_assets.mjs is flashed to the `assets` partition and memory-mapped:
// every lv_image_dsc_t points straight into flash, so sprites cost no PSRAM.
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include "audio/color_pop_score.hpp"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "game/color_pop.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/i2s_audio.hpp"
#include "little_arcade/board_io.hpp"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/settings/settings_store.hpp"
#include "lvgl.h"
#include "pack_signature.hpp"
#include "ui/asset_pack.hpp"
#include "ui/color_pop_view.hpp"

namespace {
constexpr const char* tag = "color_pop";
constexpr int screen_w = 320, screen_h = 240, buffer_lines = 24;
constexpr size_t buffer_bytes = screen_w * buffer_lines * 2;
constexpr uint32_t refresh_ms = 20;     // frame cadence (50 FPS target; CP-5 needs >= 30 FPS p95)
constexpr uint32_t touch_poll_ms = 5;   // NS2009 poll: two 100 kHz I2C transactions, ~0.7 ms
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
  if (std::memcmp(header, "CPOP", 4) != 0 || bytes != color_pop::pack::bytes ||
      std::memcmp(header + 16, color_pop::pack::signature, 16) != 0 || bytes > part->size) {
    ESP_LOGE(tag, "assets partition does not hold this build's color_pop.pack (expected %u bytes); flash it with flash.ps1",
             static_cast<unsigned>(color_pop::pack::bytes));
    return nullptr;
  }
  const void* ptr = nullptr;
  esp_partition_mmap_handle_t handle;
  if (esp_partition_mmap(part, 0, bytes, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK) {
    ESP_LOGE(tag, "assets mmap failed");
    return nullptr;
  }
  *size = bytes;
  return static_cast<const uint8_t*>(ptr);
}

const char* phase_name(color_pop::Phase p) {
  switch (p) {
    case color_pop::Phase::Start: return "START";
    case color_pop::Phase::Intro: return "INTRO";
    case color_pop::Phase::Play: return "PLAY";
    case color_pop::Phase::Celebrate: return "CELEBRATE";
  }
  return "?";
}

// Per-window frame statistics: render time (lv_timer_handler incl. waiting for SPI DMA) and the
// interval between rendered frames, both p95, plus the largest dirty fraction.
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
  void report(const color_pop::Game& game, color_pop::ColorPopScore& score) {
    const uint32_t r50 = pct(render_us, n, 50), r95 = pct(render_us, n, 95);
    const uint32_t i50 = pct(interval_us, ni, 50), i95 = pct(interval_us, ni, 95);
    const uint32_t d50 = pct(dirty_px, n, 50), d95 = pct(dirty_px, n, 95);
    const unsigned l = loops ? loops : 1, f = n ? static_cast<unsigned>(n) : 1;
    constexpr uint32_t screen = screen_w * screen_h;
    ESP_LOGI(tag,
             "[frames] n=%u render p50=%u p95=%u max=%u us | interval p50=%u p95=%u us (p95 %.1f fps) | dirty p50=%u%% "
             "p95=%u%% max=%u%% flushes/frame=%u spi_wait/frame=%u us | loop avg us: touch=%u tick=%u sync=%u | "
             "phase=%s round=%d successes=%d parts=%u score_drops=%u stack_free=%u",
             static_cast<unsigned>(n), static_cast<unsigned>(r50), static_cast<unsigned>(r95),
             static_cast<unsigned>(max_render_us), static_cast<unsigned>(i50), static_cast<unsigned>(i95),
             i95 ? 1e6 / i95 : 0.0, static_cast<unsigned>(d50 * 100 / screen), static_cast<unsigned>(d95 * 100 / screen),
             static_cast<unsigned>(max_dirty * 100 / screen), static_cast<unsigned>(flushes / f),
             static_cast<unsigned>(block_us / f), static_cast<unsigned>(touch_us / l),
             static_cast<unsigned>(tick_us / l), static_cast<unsigned>(sync_us / l),
             phase_name(game.phase()), game.round(), game.successes(), static_cast<unsigned>(game.particles().size()),
             static_cast<unsigned>(score.dropped()),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    n = ni = 0;
    max_render_us = max_dirty = loops = 0;
    touch_us = tick_us = sync_us = flushes = block_us = 0;
  }
};

#ifdef COLOR_POP_BENCH
// Full-screen render cost with groups of screen children hidden (indices follow View::create
// order). Only hides objects that are currently visible and restores them afterwards.
void profile_layers(lv_display_t* display) {
  struct Group { const char* name; int first, last; };
  static constexpr Group kGroups[] = {
      {"all", -1, -1}, {"-bubbles", 4, 21}, {"-crab", 22, 27}, {"-fx", 29, 29},
      {"-clouds+shimmer", 1, 3}, {"-badge+chrome", 30, 33}, {"bg_only", 1, 35}};
  lv_obj_t* screen = lv_screen_active();
  const int count = static_cast<int>(lv_obj_get_child_count(screen));
  char line[256];
  int len = std::snprintf(line, sizeof line, "[profile] full-screen render us:");
  for (const Group& g : kGroups) {
    bool hidden[64] = {};
    for (int i = g.first; i >= 0 && i <= g.last && i < count && i < 64; ++i) {
      lv_obj_t* child = lv_obj_get_child(screen, i);
      if (!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) { lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN); hidden[i] = true; }
    }
    uint32_t best = UINT32_MAX;
    for (int rep = 0; rep < 3; ++rep) {
      lv_obj_invalidate(screen);
      const int64_t t0 = esp_timer_get_time();
      lv_refr_now(display);
      best = std::min(best, static_cast<uint32_t>(esp_timer_get_time() - t0));
    }
    for (int i = 0; i < 64 && i < count; ++i)
      if (hidden[i]) lv_obj_remove_flag(lv_obj_get_child(screen, i), LV_OBJ_FLAG_HIDDEN);
    len += std::snprintf(line + len, sizeof line - len, " %s=%u", g.name, static_cast<unsigned>(best));
  }
  ESP_LOGI(tag, "%s", line);
}
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
           static_cast<unsigned>(color_pop::pack::bytes), static_cast<unsigned>(color_pop::pack::entries),
#ifdef COLOR_POP_BENCH
           " [BENCH: auto-taps targets]"
#else
           ""
#endif
  );
  memory_log("boot");
  size_t pack_size = 0;
  const uint8_t* pack_data = map_pack(&pack_size);
  static color_pop::AssetPack pack;
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
  // Rendering is driven explicitly with lv_refr_now() once per frame (see the loop), so keep
  // LVGL's own refresh timer from firing between frames with half-synced state.
  lv_timer_set_period(lv_display_get_refr_timer(display), 3600u * 1000u);
  const esp_err_t touch = little_arcade::board::touch_init();
  if (touch != ESP_OK) ESP_LOGE(tag, "touch unavailable: %s", esp_err_to_name(touch));

  // Audio objects first so the score exists before the game can call it; sound starts silent
  // (master ramps 0 -> 0.35 like initAudio) and only after the child presses Play.
  static color_pop::ColorPopScore score;
  static little_arcade::audio::AudioFeedback feedback(speaker_config());
  feedback.attach_score(&score);
  static color_pop::Game game(score, 1234);  // reference default (?seed absent)
  memory_log("before_view");
  static color_pop::View view;
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
  options.mic_level_probe = false;  // no microphone use: the mic icon is decorative/disabled
  if (!little_arcade::audio::start_i2s_audio(feedback, options))
    ESP_LOGE(tag, "audio unavailable; the game stays fully playable by touch with visual cues");
  ESP_LOGI(tag, "saved parent volume=%u (0 = muted); ready", volume);

  static FrameStats stats;
  bool pressed = false;
  uint32_t last = clock_ms(), report = last, last_frame = 0, last_touch = 0;
#ifdef COLOR_POP_BENCH
  uint32_t next_tap = last + 1500;
  bool profile_due = false;
#endif
  for (;;) {
    const uint32_t now = clock_ms();
    const int64_t loop_t0 = esp_timer_get_time();
    if (touch == ESP_OK && now - last_touch >= touch_poll_ms) {
      last_touch = now;
      little_arcade::board::TouchSample sample{};
      if (little_arcade::board::touch_read(&sample) == ESP_OK && sample.pressed) {
        const auto point = little_arcade::board::ns2009_to_screen(sample.raw_x, sample.raw_y);
        if (!pressed) game.down(point.x, point.y);  // the reference reacts to pointerdown only
        pressed = true;
      } else {
        pressed = false;
      }
    }
#ifdef COLOR_POP_BENCH
    if (now >= next_tap) {
      next_tap = now + 700;
      if (game.phase() == color_pop::Phase::Start) game.down(160, 120);
      else if (game.phase() == color_pop::Phase::Play)
        for (const auto& b : game.bubbles())
          if (b.color == game.target() && b.state == color_pop::BubbleState::Float) { game.down(b.x, b.y); break; }
    }
#endif
    const int64_t touch_t1 = esp_timer_get_time();
    // Tick, sync and render once per frame; touch is polled every iteration in between.
    if (now - last_frame < refresh_ms) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
    last_frame = now;
    const double dt = std::min(0.05, (now - last) / 1000.0);  // reference frame(): dt capped at 50 ms
    last = now;
    if (dt > 0) game.tick(dt);
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
#ifdef COLOR_POP_BENCH
    if (profile_due && game.phase() == color_pop::Phase::Play && game.bubbles().size() >= 4) {
      profile_due = false;
      profile_layers(display);
      last = last_frame = clock_ms();
      stats.last_frame_us = 0;  // keep the profiling pause out of the interval statistics
    }
#endif
    if (now - report >= 10000) {
      stats.report(game, score);
#ifdef COLOR_POP_BENCH
      profile_due = true;
#endif
      memory_log("playing");
      report = now;
    }
    lv_timer_handler();  // housekeeping only; rendering is driven by lv_refr_now above
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
}  // namespace

extern "C" void app_main() {
  BaseType_t created = xTaskCreatePinnedToCore(run, "color_pop_ui", 24576, nullptr, 4, nullptr, 1);
  configASSERT(created == pdPASS);
}
