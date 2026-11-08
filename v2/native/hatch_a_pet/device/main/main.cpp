#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/i2s_audio.hpp"
#include "little_arcade/board_io.hpp"
#include "little_arcade/board_pins.hpp"
#include "little_arcade/game_id.hpp"
#include "little_arcade/settings/settings_store.hpp"
#include "lvgl.h"
#include "model.hpp"
#include "pack_signature.hpp"
#include "ui/asset_pack.hpp"
#include "view.hpp"

namespace {
constexpr const char* tag="hatch_a_pet";
constexpr int w=320, h=240;
constexpr size_t buffer_bytes=w*24*2;
esp_lcd_panel_handle_t panel=nullptr;
uint64_t dirty_pixels=0;
uint32_t clock_ms() {return static_cast<uint32_t>(esp_timer_get_time()/1000);}
void done(void* ctx) {lv_display_flush_ready(static_cast<lv_display_t*>(ctx));}
void flush(lv_display_t*,const lv_area_t* area,uint8_t* pixels) {
  const uint32_t count=lv_area_get_width(area)*lv_area_get_height(area);
  dirty_pixels+=count;
  lv_draw_sw_rgb565_swap(pixels,count);
  esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels);
}
void memory_log(const char* label) {
  ESP_LOGI(tag,"[mem:%s] internal=%u min=%u psram=%u min=%u",label,
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
    static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
    static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}
const uint8_t* map_pack(size_t& size) {
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"assets");
  if(!part) {ESP_LOGE(tag,"assets partition missing");return nullptr;}
  uint8_t header[32];
  if(esp_partition_read(part,0,header,sizeof header)!=ESP_OK||
     std::memcmp(header,"CPOP",4)!=0) {
    ESP_LOGE(tag,"Hatch sprite pack missing; flash matching pack");return nullptr;
  }
  uint32_t length;
  std::memcpy(&length,header+12,4);
  if(length!=hatch_a_pet::pack::bytes||length>part->size||length<32||
     std::memcmp(header+16,hatch_a_pet::pack::signature,16)!=0) {
    ESP_LOGE(tag,"firmware/asset signature mismatch; flash matching pack");return nullptr;
  }
  const void* mapped=nullptr;
  esp_partition_mmap_handle_t handle;
  if(esp_partition_mmap(part,0,length,ESP_PARTITION_MMAP_DATA,&mapped,&handle)!=ESP_OK) {
    ESP_LOGE(tag,"assets mmap failed");return nullptr;
  }
  size=length;
  return static_cast<const uint8_t*>(mapped);
}
little_arcade::VolumeLimiter::Config speaker_config() {
  little_arcade::VolumeLimiter::Config cfg;
  cfg.sample_rate_hz=little_arcade::audio::kSampleRateHz;
  cfg.peak_limit=little_arcade::VolumeLimiter::amp_safe_peak(
    little_arcade::board::audio::kAmpGainDb,little_arcade::board::audio::kAmpSupplyMv);
  return cfg;
}
void sound(little_arcade::audio::AudioFeedback& audio,hatch_a_pet::CueEvent e) {
  using little_arcade::Cue;
  using hatch_a_pet::CueId;
  switch(e.id) {
    case CueId::MusicStart:
      audio.set_scene(static_cast<uint8_t>(little_arcade::GameId::HATCH_A_PET));
      audio.play(Cue::AMBIENT,0);break;
    case CueId::MusicStop:
      audio.set_scene(little_arcade::IFeedback::kSceneMenu);break;
    case CueId::EggLand: audio.play(Cue::NOTE,0x41);break;
    case CueId::PromptKnock: audio.play(Cue::NOTE,0x21);break;
    case CueId::EggHint: audio.play(Cue::NOTE,0x20);break;
    case CueId::Knock: audio.play(Cue::NOTE,0x10|e.variant);break;
    case CueId::PreHatchTrill: audio.play(Cue::NOTE,0x30);break;
    case CueId::HatchPop: audio.play(Cue::SUCCESS,0);break;
    case CueId::CreatureChirp: audio.play(Cue::REACTION,0x30|e.variant);break;
    case CueId::PetGiggle: audio.play(Cue::REACTION,e.variant);break;
    case CueId::PetJump: audio.play(Cue::REACTION,0x10|e.variant);break;
    case CueId::PetSneeze: audio.play(Cue::REACTION,0x20|e.variant);break;
    case CueId::ChoiceAppear: audio.play(Cue::NOTE,0x43);break;
    case CueId::ChoiceYes: case CueId::ChoiceNo: audio.play(Cue::NOTE,0x42);break;
    case CueId::WaveBye: audio.play(Cue::REACTION,0x40|e.variant);break;
    case CueId::ShelfWave: audio.play(Cue::NOTE,0x50|e.variant);break;
    case CueId::EmptyTap: audio.play(Cue::NOTE,0x44);break;
  }
}
void run(void*) {
  uint32_t flash=0;
  ESP_ERROR_CHECK(esp_flash_get_size(nullptr,&flash));
  ESP_LOGI(tag,"flash=%" PRIu32 " psram=%u",flash,
           static_cast<unsigned>(esp_psram_is_initialized()?esp_psram_get_size():0));
  memory_log("boot");
  size_t pack_size=0;
  const uint8_t* mapped=map_pack(pack_size);
  static color_pop::AssetPack pack;
  std::string error;
  if(!mapped||!pack.load(mapped,pack_size,&error)) {
    ESP_LOGE(tag,"asset pack unusable: %s",error.c_str());
    vTaskDelete(nullptr);return;
  }
  ESP_LOGI(tag,"pack=%u bytes sprites=%u",static_cast<unsigned>(pack_size),
           static_cast<unsigned>(pack.count()));
  lv_init();
  lv_tick_set_cb(clock_ms);
  auto* display=lv_display_create(w,h);
  little_arcade::board::DisplayConfig cfg;
  cfg.pclk_hz=40*1000*1000;
  cfg.max_transfer_bytes=buffer_bytes;
  cfg.on_flush_done=done;
  cfg.ctx=display;
  ESP_ERROR_CHECK(little_arcade::board::display_init(cfg,&panel));
  void* first=heap_caps_malloc(buffer_bytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  void* second=heap_caps_malloc(buffer_bytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  configASSERT(first&&second);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,first,second,buffer_bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,flush);
  lv_timer_set_period(lv_display_get_refr_timer(display),3600u*1000u);
  const esp_err_t touch=little_arcade::board::touch_init();
  if(touch!=ESP_OK)ESP_LOGE(tag,"touch unavailable: %s",esp_err_to_name(touch));
  static hatch_a_pet::Model model(1234);
  static hatch_a_pet::View view;
  const char* missing=nullptr;
  if(!view.create(lv_screen_active(),pack,&missing)) {
    ESP_LOGE(tag,"missing sprite %s",missing?missing:"unknown");
    vTaskDelete(nullptr);return;
  }
  view.sync(model.state(),false);
  lv_refr_now(display);
  memory_log("first_frame");
  little_arcade::board::backlight_set(true);

  little_arcade::settings::init_store();
  static little_arcade::audio::AudioFeedback audio(speaker_config());
  audio.set_level(little_arcade::settings::load_volume_level());
  little_arcade::audio::I2sAudioOptions options;
  options.mic_level_probe=false;
  if(!little_arcade::audio::start_i2s_audio(audio,options))
    ESP_LOGE(tag,"audio unavailable; touch remains complete");
  bool pressed=false;
  uint32_t last=clock_ms(),frame=last,report=last,poll=last;
  uint32_t samples=0,worst_us=0,worst_interval=0,last_render=0;
  uint64_t max_dirty=0;
  for(;;) {
    const uint32_t now=clock_ms();
    if(touch==ESP_OK&&now-poll>=5) {
      poll=now;
      little_arcade::board::TouchSample sample{};
      if(little_arcade::board::touch_read(&sample)==ESP_OK&&sample.pressed) {
        const auto point=little_arcade::board::ns2009_to_screen(sample.raw_x,sample.raw_y);
        if(!pressed)model.pointer_down(point.x,point.y);
        pressed=true;
      } else {
        if(pressed)model.pointer_up();
        pressed=false;
      }
    }
    hatch_a_pet::CueEvent cue;
    while(model.pop_cue(cue))sound(audio,cue);
    if(now-frame<20){vTaskDelay(pdMS_TO_TICKS(1));continue;}
    frame=now;
    model.tick(std::min(0.05,(now-last)/1000.0));
    last=now;
    view.sync(model.state(),false);
    const uint64_t before=dirty_pixels;
    const int64_t begin=esp_timer_get_time();
    lv_refr_now(display);
    const uint32_t us=static_cast<uint32_t>(esp_timer_get_time()-begin);
    worst_us=std::max(worst_us,us);
    max_dirty=std::max(max_dirty,dirty_pixels-before);
    if(last_render)worst_interval=std::max(worst_interval,now-last_render);
    last_render=now;
    ++samples;
    if(now-report>=10000) {
      ESP_LOGI(tag,"[frames] n=%u max_render_us=%u max_interval_ms=%u max_dirty_px=%u "
                   "phase=%u audio_peak=%" PRId32,
        samples,worst_us,worst_interval,static_cast<unsigned>(max_dirty),
        static_cast<unsigned>(model.state().phase),audio.peak_since_reset());
      memory_log("playing");
      samples=worst_us=worst_interval=0;max_dirty=0;report=now;
    }
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
}
extern "C" void app_main() {
  const BaseType_t created=xTaskCreatePinnedToCore(run,"hatch_ui",24576,nullptr,4,nullptr,1);
  configASSERT(created==pdPASS);
}
