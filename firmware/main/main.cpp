// Little Arcade firmware: launcher plus the implemented scenes on H02 display/touch.
// Single UI task owns LVGL (docs/architecture.md). Sound is muted until a parent
// opts in (parent gate). Touch presses are logged raw + mapped for calibration checks.
#include <cinttypes>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "little_arcade/arcade.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/i2s_audio.hpp"
#include "little_arcade/audio/sampler.hpp"
#include "little_arcade/board_io.hpp"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/intent.hpp"
#include "little_arcade/settings/settings_store.hpp"
#include "lvgl.h"

namespace {
using namespace little_arcade;

constexpr const char* kTag = "little_arcade";
constexpr uint32_t kStatsPeriodMs = 10000;
constexpr int kBufLines = 24;  // two partial RGB565 buffers, 15 KB each
// SPI clock spike: upstream TFT_eSPI uses 27 MHz (80/3); 40 MHz (80/2) is
// within the ST7789's 16 ns write cycle. Pending on-device visual check.
constexpr uint32_t kLcdSpiHz = 40 * 1000 * 1000;
constexpr std::size_t kBufBytes = kScreenWidth * kBufLines * 2;

void log_memory(const char* phase) {
  lv_mem_monitor_t lv{};
  // lv_mem_monitor divides by the pool size, which is zero before lv_init().
  if (lv_is_initialized()) lv_mem_monitor(&lv);
  ESP_LOGI(kTag,
           "[mem:%s] internal free=%u min=%u largest=%u | psram free=%u min=%u | lvgl used=%u max=%u total=%u",
           phase, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(lv.total_size - lv.free_size), static_cast<unsigned>(lv.max_used),
           static_cast<unsigned>(lv.total_size));
}

void log_board() {
  namespace b = little_arcade::board;
  esp_chip_info_t chip{};
  esp_chip_info(&chip);
  uint32_t flash_bytes = 0;
  esp_flash_get_size(nullptr, &flash_bytes);
  const esp_app_desc_t* app = esp_app_get_description();

  ESP_LOGI(kTag, "app=%s idf=%s cores=%d rev=%d.%d", app->version, app->idf_ver, chip.cores,
           chip.revision / 100, chip.revision % 100);
  ESP_LOGI(kTag, "flash=%" PRIu32 " bytes, psram=%u bytes", flash_bytes,
           static_cast<unsigned>(esp_psram_is_initialized() ? esp_psram_get_size() : 0));
  ESP_LOGI(kTag, "lvgl=%d.%d.%d intents=%u", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           LVGL_VERSION_PATCH, static_cast<unsigned>(little_arcade::kIntentCount));
  ESP_LOGW(kTag, "board pins: verified=%s level=%s schematic=%s pcb=%s",
           b::kPinsVerified ? "true" : "false", b::kVerificationLevel, b::kSchematicRevision,
           b::kPhysicalPcbRevision);
  if (!b::kPinsVerified) {
    ESP_LOGW(kTag, "H02 bring-up: starting display/touch on source-reviewed pins.");
  }
}

// Volume policy (docs/acceptance.md G4, AGENTS.md rule 9): output is muted
// until a parent opts in through the hidden parent gate (hold the Mic corner
// 5 s, answer a sum, pick 0..6). The chosen level is saved in NVS and applied
// only after the first frame, ramping over 300 ms: no startup sound.
// H03 bring-up: log INMP441 RMS/peak numbers only (no samples kept or sent).
constexpr bool kMicLevelProbe = true;
// H03 diagnostic only, keep false in normal builds: ~10 s after boot plays six
// soft menu chimes at the parent-saved level (silent while muted) so the
// [audio] mic RMS log can show acoustic loopback from the speaker to the INMP441.
constexpr bool kAudioLoopbackSelfTest = false;
// Device bench: opens Color Pop 3 s after boot and pops the target bubble
// every 1.5 s, so the perf log shows real frame cost without a tester.
constexpr bool kBenchColorPop = false;

void bench_color_pop(Arcade& arcade, uint32_t now) {
  static uint32_t next = 3000;
  if (now < next) return;
  next = now + 1500;
  if (arcade.layer() != Arcade::Layer::COLOR_POP) {
    const Rect& r = SceneManager::kTileRects[0];
    arcade.touch(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), now);
    return;
  }
  const color_pop::ColorPopGame& g = arcade.color_pop();
  for (std::size_t i = 0; i < g.bubble_count(); ++i) {
    const color_pop::Bubble& b = g.bubble(i);
    if (b.color == g.target() && b.state == color_pop::BubbleState::FLOAT) {
      arcade.touch(static_cast<int16_t>(b.x), static_cast<int16_t>(b.y), now);
      return;
    }
  }
}

// Device bench: opens Hatch-a-Pet 3 s after boot and plays every phase (knock,
// hatch, three reactions, YES to the shelf, idle hint on every third egg).
constexpr bool kBenchHatchAPet = false;

void bench_hatch_a_pet(Arcade& arcade, uint32_t now) {
  using hatch_a_pet::HatchAPetGame;
  using hatch_a_pet::Phase;
  static uint32_t next = 3000;
  static uint32_t eggs = 0;
  static Phase last = Phase::START;
  static bool badged = false;
  if (now < next) return;
  next = now + 700;
  if (arcade.layer() != Arcade::Layer::HATCH_A_PET) {
    const Rect& r = SceneManager::kTileRects[1];
    arcade.touch(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), now);
    return;
  }
  const HatchAPetGame& g = arcade.hatch_a_pet();
  if (g.phase() != last) {
    if (g.phase() == Phase::EGG) {
      ++eggs;
      badged = false;
      if (eggs % 3 == 0) next = now + 13000;  // let the 6 s glow and 12 s hand hints play
    }
    last = g.phase();
    return;
  }
  const auto tap = [&](const Rect& r) {
    arcade.touch(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), now);
  };
  if (g.phase() == Phase::EGG) {
    if (!badged && eggs % 2) {
      tap(HatchAPetGame::kPromptBadge);
      badged = true;
    } else {
      tap(HatchAPetGame::kEggHit);
    }
  } else if (g.phase() == Phase::PET) {
    if (g.choices().show && g.choices().t > 0.5) {
      tap(HatchAPetGame::kYesButton);
    } else if (!g.choices().show) {
      tap(HatchAPetGame::kCreature);
      next = now + 1100;
    }
  }
}

const char* layer_name(Arcade::Layer l) {
  switch (l) {
    case Arcade::Layer::MENU: return "menu";
    case Arcade::Layer::ANIMAL_SOUNDS: return "animal_sounds";
    case Arcade::Layer::COLOR_POP: return "color_pop";
    case Arcade::Layer::HATCH_A_PET: return "hatch_a_pet";
    case Arcade::Layer::RAINBOW_PIANO: return "rainbow_piano";
    case Arcade::Layer::ROBO_BUDDY: return "robo_buddy";
    case Arcade::Layer::SHAPE_GARAGE: return "shape_garage";
    case Arcade::Layer::FEED_MONSTER: return "feed_monster";
    case Arcade::Layer::SOUND_JAR: return "sound_jar";
    case Arcade::Layer::BEAT_SHARKS: return "beat_sharks";
    case Arcade::Layer::BUBBLE_AQUARIUM: return "bubble_aquarium";
    case Arcade::Layer::ROCKET_RESCUE: return "rocket_rescue";
    case Arcade::Layer::SILLY_FACES: return "silly_faces";
    case Arcade::Layer::NONE: break;
  }
  return "none";
}

// Speaker ceiling follows the amp GAIN strap and supply in config/board_pins.json,
// so a louder strap lowers the digital peak instead of clipping the amplifier.
VolumeLimiter::Config speaker_config() {
  VolumeLimiter::Config cfg;
  cfg.sample_rate_hz = audio::kSampleRateHz;
  cfg.peak_limit = VolumeLimiter::amp_safe_peak(board::audio::kAmpGainDb, board::audio::kAmpSupplyMv);
  return cfg;
}

audio::AudioFeedback g_feedback(speaker_config());
Arcade* g_arcade = nullptr;

// Sound Jar's sample buffers live in PSRAM (about 200 KB). They hold at most
// one short clip, only in RAM, and are zeroed on wipe; never written anywhere.
audio::Sampler* make_sampler() {
  constexpr std::size_t kClipBytes = audio::Sampler::kMaxSamples * sizeof(int16_t);
  constexpr std::size_t kEchoBytes = audio::Sampler::kEchoSamples * sizeof(int16_t);
  auto* clip = static_cast<int16_t*>(heap_caps_malloc(kClipBytes, MALLOC_CAP_SPIRAM));
  auto* record = static_cast<int16_t*>(heap_caps_malloc(kClipBytes, MALLOC_CAP_SPIRAM));
  auto* echo = static_cast<int16_t*>(heap_caps_malloc(kEchoBytes, MALLOC_CAP_SPIRAM));
  if (!clip || !record || !echo) {
    heap_caps_free(clip);
    heap_caps_free(record);
    heap_caps_free(echo);
    ESP_LOGE(kTag, "sound jar buffers unavailable; Sound Jar runs without sound");
    return nullptr;
  }
  static audio::Sampler sampler(clip, record, echo);
  return &sampler;
}
esp_lcd_panel_handle_t g_panel = nullptr;
uint64_t g_frame_pixels = 0;
bool g_was_pressed = false;

uint32_t tick_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void audio_loopback_self_test(uint32_t now) {
  static const uint32_t start = now;
  static uint32_t next = 10000;
  static int played = 0;
  const uint32_t t = now - start;
  if (played < 6 && t >= next) {
    g_feedback.play(Cue::SUCCESS, 0);
    ESP_LOGI("audio", "[selftest] chime %d at %u ms", ++played, static_cast<unsigned>(t));
    next = t + 900;
  }
}
void flush_done(void* ctx) { lv_display_flush_ready(static_cast<lv_display_t*>(ctx)); }

void flush(lv_display_t*, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(px_map, static_cast<uint32_t>(w * h));  // ST7789 expects big-endian RGB565
  g_frame_pixels += static_cast<uint64_t>(w * h);
  esp_lcd_panel_draw_bitmap(g_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

// Polled from the UI loop; each press edge becomes one Arcade touch (the
// SceneManager debounces). LVGL has no input device, so no widget ever
// swallows a press. Raw + mapped points are logged for calibration checks.
void poll_touch(uint32_t now) {
  board::TouchSample s{};
  if (board::touch_read(&s) != ESP_OK || !s.pressed) {
    g_was_pressed = false;
    g_arcade->pointer(false, 0, 0, now);
    return;
  }
  const board::ScreenPoint p = board::ns2009_to_screen(s.raw_x, s.raw_y);
  if (!g_was_pressed) {
    ESP_LOGI("touch", "down raw=(%u,%u) z=%u -> screen=(%d,%d)", s.raw_x, s.raw_y, s.raw_z, p.x, p.y);
  }
  g_was_pressed = true;
  // Every sample goes to the Arcade: it needs held presses for the parent
  // gate and forwards only new presses to the launcher/games.
  g_arcade->pointer(true, p.x, p.y, now);
}
lv_display_t* start_display() {
  lv_display_t* disp = lv_display_create(kScreenWidth, kScreenHeight);
  board::DisplayConfig cfg;
  cfg.pclk_hz = kLcdSpiHz;
  cfg.max_transfer_bytes = kBufBytes;
  cfg.on_flush_done = flush_done;
  cfg.ctx = disp;
  ESP_ERROR_CHECK(board::display_init(cfg, &g_panel));

  void* b1 = heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* b2 = heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  configASSERT(b1 && b2);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, b1, b2, kBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush);
  return disp;
}

}  // namespace

constexpr uint32_t kUiStackBytes = 16 * 1024;  // LVGL SW render overflowed the 3.5 KB main stack

void ui_task(void*) {
  lv_init();
  lv_tick_set_cb(tick_ms);
  start_display();

  const esp_err_t touch_err = board::touch_init();
  if (touch_err != ESP_OK) ESP_LOGE(kTag, "touch init failed (%s); display only", esp_err_to_name(touch_err));

  settings::init_store();
  const uint8_t saved_level = settings::load_volume_level();
  ESP_LOGI(kTag, "parent volume level=%u (%s)", static_cast<unsigned>(saved_level),
           saved_level ? "saved by parent" : "muted until a parent opts in");
  ESP_LOGI(kTag, "speaker ceiling=%d (amp gain %u dB, supply %u mV)",
           static_cast<int>(speaker_config().peak_limit), static_cast<unsigned>(board::audio::kAmpGainDb),
           static_cast<unsigned>(board::audio::kAmpSupplyMv));
  const bool saved_mic = settings::load_mic_enabled();
  static Arcade arcade(g_feedback, esp_random() | 1u, saved_level, saved_mic);
  g_arcade = &arcade;
  const int64_t create_t0 = esp_timer_get_time();
  arcade.create(lv_screen_active());
  ESP_LOGI(kTag, "arcade created in %lld ms", (esp_timer_get_time() - create_t0) / 1000);
  log_memory("arcade_created");

  // First full frame before the backlight comes on: no flash of garbage.
  arcade.tick(tick_ms());
  lv_refr_now(nullptr);
  board::backlight_set(true);
  log_memory("ui_ready");

  audio::Sampler* sampler = make_sampler();
  if (sampler) {
    sampler->set_mic_allowed(saved_mic);
    g_feedback.attach_sampler(sampler);  // before the audio task starts mixing
  }
  audio::I2sAudioOptions audio_opts;
  audio_opts.mic_level_probe = kMicLevelProbe;
  audio_opts.sampler = sampler;
  const bool audio_ok = audio::start_i2s_audio(g_feedback, audio_opts);
  if (!audio_ok) ESP_LOGE(kTag, "audio unavailable; games stay fully playable with visual cues");
  // Without a running audio task nothing would service the sampler.
  arcade.attach_sampler(audio_ok ? sampler : nullptr);
  ESP_LOGI(kTag, "sound jar: sampler=%s mic=%s (parent switch %s)", sampler ? "yes" : "no",
           sampler && sampler->mic_available() ? "available" : "off", saved_mic ? "on" : "off");
  int applied_level = -1;
  log_memory("audio_ready");

  uint32_t stats_start = tick_ms(), frames = 0;
  int64_t render_us_sum = 0, render_us_max = 0, tick_us_max = 0;
  uint64_t pixels_sum = 0;
  Arcade::Layer last_layer = arcade.layer();
  for (;;) {
    const uint32_t now = tick_ms();
    if (touch_err == ESP_OK) poll_touch(now);
    if (audio_ok && arcade.volume_level() != applied_level) {
      applied_level = arcade.volume_level();
      g_feedback.set_level(static_cast<uint8_t>(applied_level));  // ramps; 0 = muted
      ESP_LOGI("audio", "volume level %d applied", applied_level);
    }
    if (arcade.take_volume_changed()) settings::save_volume_level(arcade.volume_level());
    if (arcade.take_mic_changed()) {
      if (sampler) sampler->set_mic_allowed(arcade.mic_enabled());
      settings::save_mic_enabled(arcade.mic_enabled());
    }
    if (kAudioLoopbackSelfTest) audio_loopback_self_test(now);
    if (kBenchColorPop) bench_color_pop(arcade, now);
    if (kBenchHatchAPet) bench_hatch_a_pet(arcade, now);
    const int64_t t_tick = esp_timer_get_time();
    arcade.tick(now);  // game logic + view sync
    const int64_t tick_us = esp_timer_get_time() - t_tick;
    tick_us_max = tick_us > tick_us_max ? tick_us : tick_us_max;
    if (arcade.layer() != last_layer) {
      last_layer = arcade.layer();
      ESP_LOGI(kTag, "[nav] layer=%s", layer_name(last_layer));
      log_memory("layer");
    }
    g_frame_pixels = 0;
    const int64_t t0 = esp_timer_get_time();
    const uint32_t idle_ms = lv_timer_handler();
    const int64_t dt = esp_timer_get_time() - t0;
    if (g_frame_pixels) {
      ++frames;
      render_us_sum += dt;
      render_us_max = dt > render_us_max ? dt : render_us_max;
      pixels_sum += g_frame_pixels;
    }
    if (now - stats_start >= kStatsPeriodMs) {
      lv_mem_monitor_t mem;
      lv_mem_monitor(&mem);
      hatch_a_pet::HatchAPetView& hv = arcade.hatch_a_pet_view();
      color_pop::ColorPopView& cv = arcade.color_pop_view();
      if (arcade.layer() == Arcade::Layer::COLOR_POP) {
        ESP_LOGI("perf", "[device] color_pop syncs=%u frames=%u tiles/frame=%u (core0 %u%%) paint avg=%u max=%u us, raster avg=%u max=%u us",
                 static_cast<unsigned>(cv.syncs()), static_cast<unsigned>(cv.frames()),
                 static_cast<unsigned>(cv.frames() ? cv.tiles_drawn() / cv.frames() : 0),
                 static_cast<unsigned>(cv.tiles_drawn() ? cv.tiles_by_worker() * 100 / cv.tiles_drawn() : 0),
                 static_cast<unsigned>(cv.paint_us_avg()), static_cast<unsigned>(cv.paint_us_max()),
                 static_cast<unsigned>(cv.render_us_avg()), static_cast<unsigned>(cv.render_us_max()));
      }
      if (arcade.layer() == Arcade::Layer::HATCH_A_PET) {
        ESP_LOGI("perf", "[device] hatch_a_pet syncs=%u frames=%u tiles/frame=%u (core0 %u%%) paint avg=%u max=%u us, raster avg=%u max=%u us",
                 static_cast<unsigned>(hv.syncs()), static_cast<unsigned>(hv.frames()),
                 static_cast<unsigned>(hv.frames() ? hv.tiles_drawn() / hv.frames() : 0),
                 static_cast<unsigned>(hv.tiles_drawn() ? hv.tiles_by_worker() * 100 / hv.tiles_drawn() : 0),
                 static_cast<unsigned>(hv.paint_us_avg()), static_cast<unsigned>(hv.paint_us_max()),
                 static_cast<unsigned>(hv.render_us_avg()), static_cast<unsigned>(hv.render_us_max()));
      }
      cv.reset_stats();
      hv.reset_stats();
      ESP_LOGI("perf",
               "[device] layer=%s %u redraws/10s, render avg=%lld us max=%lld us, tick max=%lld us, px avg=%llu, "
               "pop rounds=%u, hatches=%u, lv_mem max_used=%u, ui stack free=%u",
               layer_name(arcade.layer()), static_cast<unsigned>(frames), frames ? render_us_sum / frames : 0LL,
               render_us_max, tick_us_max, frames ? pixels_sum / frames : 0ULL,
               static_cast<unsigned>(arcade.color_pop().rounds_completed()),
               static_cast<unsigned>(arcade.hatch_a_pet().hatch_count()),
               static_cast<unsigned>(mem.max_used), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
      log_memory("ui");
      stats_start = now;
      frames = 0;
      render_us_sum = render_us_max = tick_us_max = 0;
      pixels_sum = 0;
    }
    // The ported canvas games repaint every tick, so their frame time is
    // tick + LVGL refresh; sleeping until the next 33 ms refresh slot only
    // adds up to ~20 ms of dead time per frame. Yield 1 ms (idle task and
    // watchdog still run) and let the overdue refresh go straight away.
    const bool canvas_game = arcade.layer() == Arcade::Layer::COLOR_POP || arcade.layer() == Arcade::Layer::HATCH_A_PET;
    vTaskDelay(pdMS_TO_TICKS(canvas_game ? 1 : (idle_ms < 5 ? 5 : (idle_ms > 20 ? 20 : idle_ms))));
  }
}
extern "C" void app_main(void) {
  log_memory("boot");
  log_board();
  xTaskCreatePinnedToCore(ui_task, "ui", kUiStackBytes, nullptr, 5, nullptr, 1);
}
