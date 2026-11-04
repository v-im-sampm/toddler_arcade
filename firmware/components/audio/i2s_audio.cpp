#include "little_arcade/audio/i2s_audio.hpp"

#include <cinttypes>
#include <cmath>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "little_arcade/board_pins.hpp"

namespace little_arcade::audio {
namespace {

constexpr char kTag[] = "audio";
constexpr std::size_t kFrames = 256;  // 16 ms per block at 16 kHz
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 6;  // above the UI task (5)

i2s_chan_handle_t g_tx = nullptr;
i2s_chan_handle_t g_rx = nullptr;
AudioFeedback* g_feedback = nullptr;
I2sAudioOptions g_options;

int16_t g_mono[kFrames];
int32_t g_tx_frames[kFrames * 2];
int32_t g_rx_frames[kFrames * 2];

void audio_task(void*) {
  int64_t last_log_us = esp_timer_get_time();
  uint64_t mic_sumsq = 0;
  uint32_t mic_count = 0;
  int32_t mic_peak = 0;
  int64_t worst_render_us = 0;
  for (;;) {
    const int64_t t0 = esp_timer_get_time();
    g_feedback->render(g_mono, kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
      // Same sample in both slots: the amp's SD_MODE strap picks one channel.
      const int32_t v = static_cast<int32_t>(g_mono[i]) * 65536;
      g_tx_frames[2 * i] = v;
      g_tx_frames[2 * i + 1] = v;
    }
    const int64_t dt = esp_timer_get_time() - t0;
    if (dt > worst_render_us) worst_render_us = dt;

    std::size_t written = 0;
    i2s_channel_write(g_tx, g_tx_frames, sizeof(g_tx_frames), &written, portMAX_DELAY);

    if (g_rx) {
      std::size_t got = 0;
      i2s_channel_read(g_rx, g_rx_frames, sizeof(g_rx_frames), &got, 0);
      const std::size_t frames = got / (2 * sizeof(int32_t));
      for (std::size_t i = 0; i < frames; ++i) {
        const int32_t s = g_rx_frames[2 * i] >> 16;  // INMP441 L/R=GND -> left slot, 24-bit MSB-first
        const int32_t a = s < 0 ? -s : s;
        if (a > mic_peak) mic_peak = a;
        mic_sumsq += static_cast<uint64_t>(static_cast<int64_t>(s) * s);
        ++mic_count;
      }
    }

    const int64_t now = esp_timer_get_time();
    if (now - last_log_us >= static_cast<int64_t>(g_options.log_period_ms) * 1000) {
      last_log_us = now;
      const int32_t out_peak = g_feedback->peak_since_reset();
      if (g_rx) {
        const unsigned rms = mic_count ? static_cast<unsigned>(std::sqrt(static_cast<double>(mic_sumsq) / mic_count)) : 0u;
        ESP_LOGI(kTag,
                 "[audio] level=%u cues=%" PRIu32 " merged=%" PRIu32 " dropped=%" PRIu32
                 " out_peak=%" PRId32 " render_max=%lld us | mic frames=%" PRIu32 " rms=%u peak=%" PRId32,
                 g_feedback->level(), g_feedback->cues_played(), g_feedback->cues_merged(),
                 g_feedback->cues_dropped(), out_peak, worst_render_us, mic_count, rms, mic_peak);
      } else {
        ESP_LOGI(kTag,
                 "[audio] level=%u cues=%" PRIu32 " merged=%" PRIu32 " dropped=%" PRIu32
                 " out_peak=%" PRId32 " render_max=%lld us",
                 g_feedback->level(), g_feedback->cues_played(), g_feedback->cues_merged(),
                 g_feedback->cues_dropped(), out_peak, worst_render_us);
      }
      mic_sumsq = 0;
      mic_count = 0;
      mic_peak = 0;
      worst_render_us = 0;
    }
  }
}

}  // namespace

bool start_i2s_audio(AudioFeedback& feedback, const I2sAudioOptions& options) {
  namespace pins = board::audio;
  if (pins::kBclk == board::kPinTbd || pins::kWs == board::kPinTbd || pins::kAmpDataOut == board::kPinTbd ||
      (options.mic_level_probe && pins::kMicDataIn == board::kPinTbd)) {
    ESP_LOGE(kTag, "audio pins are TBD in config/board_pins.json; audio disabled");
    return false;
  }
  if (g_tx) return true;
  g_feedback = &feedback;
  g_options = options;

  i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan.dma_desc_num = 4;
  chan.dma_frame_num = kFrames;
  chan.auto_clear = true;  // underrun sends silence, not stale audio
  esp_err_t err = i2s_new_channel(&chan, &g_tx, options.mic_level_probe ? &g_rx : nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "i2s_new_channel failed: %s", esp_err_to_name(err));
    return false;
  }

  // TX and RX share BCLK/WS (full duplex on one port), so both use the same
  // 16 kHz, 32-bit stereo Philips framing: 64 BCLK per frame as the INMP441
  // requires; the MAX98357A accepts 32-bit slots.
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = static_cast<gpio_num_t>(pins::kBclk),
              .ws = static_cast<gpio_num_t>(pins::kWs),
              .dout = static_cast<gpio_num_t>(pins::kAmpDataOut),
              .din = options.mic_level_probe ? static_cast<gpio_num_t>(pins::kMicDataIn) : I2S_GPIO_UNUSED,
              .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
          },
  };
  err = i2s_channel_init_std_mode(g_tx, &std_cfg);
  if (err == ESP_OK && g_rx) err = i2s_channel_init_std_mode(g_rx, &std_cfg);
  if (err == ESP_OK) err = i2s_channel_enable(g_tx);
  if (err == ESP_OK && g_rx) err = i2s_channel_enable(g_rx);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "i2s init failed: %s", esp_err_to_name(err));
    return false;
  }

  if (xTaskCreatePinnedToCore(audio_task, "audio", kTaskStack, nullptr, kTaskPriority, nullptr, 0) != pdPASS) {
    ESP_LOGE(kTag, "audio task create failed");
    return false;
  }
  ESP_LOGI(kTag, "I2S0 %" PRIu32 " Hz bclk=%d ws=%d dout=%d din=%d (wiring: %s, bench verified: %s)", kSampleRateHz,
           pins::kBclk, pins::kWs, pins::kAmpDataOut, options.mic_level_probe ? pins::kMicDataIn : -1,
           pins::kWiringSource, pins::kWiringBenchVerified ? "yes" : "no");
  return true;
}

}  // namespace little_arcade::audio
