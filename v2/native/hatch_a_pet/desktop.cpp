#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "little_arcade/audio/hatch_a_pet_score.hpp"
#include "little_arcade/volume.hpp"
#include "model.hpp"
#include "view.hpp"

namespace {
using namespace hatch_a_pet;
constexpr int w=320,h=240;
std::vector<uint8_t> bytes;
std::array<uint16_t,w*h> pixels{};
std::array<uint16_t,w*24> drawbuf{};
uint32_t sim_ms=0;
uint32_t clock_ms() { return sim_ms; }
void flush(lv_display_t* display,const lv_area_t* area,uint8_t* src) {
  const int width=lv_area_get_width(area);
  for(int y=area->y1;y<=area->y2;++y)
    std::memcpy(&pixels[y*w+area->x1],src+(y-area->y1)*width*2,width*2);
  lv_display_flush_ready(display);
}
bool screenshot(const std::string& path) {
  FILE* f=std::fopen(path.c_str(),"wb");
  if(!f) return false;
  uint8_t hd[54]{'B','M'}; uint32_t size=54+w*h*3, width=w,height=h;
  std::memcpy(hd+2,&size,4);hd[10]=54;hd[14]=40;
  std::memcpy(hd+18,&width,4);std::memcpy(hd+22,&height,4);hd[26]=1;hd[28]=24;
  std::fwrite(hd,1,54,f);
  for(int y=h-1;y>=0;--y) for(int x=0;x<w;++x) {
    uint16_t p=pixels[y*w+x];
    uint8_t bgr[]{static_cast<uint8_t>((p&31)*255/31),
      static_cast<uint8_t>(((p>>5)&63)*255/63),static_cast<uint8_t>((p>>11)*255/31)};
    std::fwrite(bgr,1,3,f);
  }
  return std::fclose(f)==0;
}
bool load(color_pop::AssetPack& pack, std::string file) {
  if(file.empty()) {
    char* base=SDL_GetBasePath();
    file=std::string(base?base:"")+"hatch_a_pet.pack";
    if(base)SDL_free(base);
  }
  std::ifstream in(file,std::ios::binary);
  if(!in) {std::fprintf(stderr,"Cannot open %s\n",file.c_str());return false;}
  bytes.assign(std::istreambuf_iterator<char>(in),{});
  std::string error;
  if(!pack.load(bytes.data(),bytes.size(),&error)) {
    std::fprintf(stderr,"Bad sprite pack: %s\n",error.c_str());return false;
  }
  return true;
}
struct Audio {
  SDL_AudioDeviceID device=0;
  little_arcade::audio::HatchAPetScore score;
  little_arcade::VolumeLimiter limiter{little_arcade::VolumeLimiter::Config{}};
  std::array<int32_t,little_arcade::VolumeLimiter::kChunk> mix{};
  static void fill(void* ptr,Uint8* dst,int len) {
    auto& a=*static_cast<Audio*>(ptr);
    auto* out=reinterpret_cast<int16_t*>(dst);
    std::size_t n=static_cast<std::size_t>(len)/2;
    while(n) {
      const auto count=std::min(n,a.mix.size());
      std::fill(a.mix.begin(),a.mix.begin()+count,0);
      a.score.render_add(a.mix.data(),count);
      a.limiter.process(a.mix.data(),out,count);
      out+=count;n-=count;
    }
  }
  void open() {
    SDL_AudioSpec want{};want.freq=16000;want.channels=1;want.format=AUDIO_S16SYS;
    want.samples=256;want.callback=fill;want.userdata=this;
    device=SDL_OpenAudioDevice(nullptr,0,&want,nullptr,0);
    if(!device) std::fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());
    else {limiter.set_level(3);SDL_PauseAudioDevice(device,0);}
  }
  void cue(CueEvent e) {
    if(!device)return;
    using little_arcade::Cue;
    Cue kind=Cue::NOTE;
    uint8_t variant=0;
    switch(e.id) {
      case CueId::MusicStart: kind=Cue::AMBIENT;break;
      case CueId::MusicStop: SDL_LockAudioDevice(device);score.stop();SDL_UnlockAudioDevice(device);return;
      case CueId::EggLand: variant=0x41;break;
      case CueId::PromptKnock: variant=0x21;break;
      case CueId::EggHint: variant=0x20;break;
      case CueId::Knock: variant=0x10|e.variant;break;
      case CueId::PreHatchTrill: variant=0x30;break;
      case CueId::HatchPop: kind=Cue::SUCCESS;break;
      case CueId::CreatureChirp: kind=Cue::REACTION;variant=0x30|e.variant;break;
      case CueId::PetGiggle: kind=Cue::REACTION;variant=e.variant;break;
      case CueId::PetJump: kind=Cue::REACTION;variant=0x10|e.variant;break;
      case CueId::PetSneeze: kind=Cue::REACTION;variant=0x20|e.variant;break;
      case CueId::ChoiceAppear: variant=0x43;break;
      case CueId::ChoiceYes:case CueId::ChoiceNo:variant=0x42;break;
      case CueId::WaveBye: kind=Cue::REACTION;variant=0x40|e.variant;break;
      case CueId::ShelfWave: variant=0x50|e.variant;break;
      case CueId::EmptyTap: variant=0x44;break;
    }
    SDL_LockAudioDevice(device);
    score.cue(kind,variant);
    SDL_UnlockAudioDevice(device);
  }
  ~Audio(){if(device)SDL_CloseAudioDevice(device);}
};
}
int main(int argc,char** argv) {
  bool check=false,mute=false,reduced=false;
  std::string pack_path,output,scene="pet";
  uint32_t seed=1234;
  int zoom=2;
  for(int i=1;i<argc;++i) {
    const std::string arg=argv[i];
    if(arg=="--check")check=true;
    else if(arg=="--mute")mute=true;
    else if(arg=="--reduced")reduced=true;
    else if((arg=="--pack"||arg=="--screenshot"||arg=="--seed"||arg=="--zoom"||arg=="--scene")&&i+1<argc) {
      const std::string value=argv[++i];
      if(arg=="--pack")pack_path=value;
      else if(arg=="--screenshot")output=value;
      else if(arg=="--seed")seed=static_cast<uint32_t>(std::strtoul(value.c_str(),nullptr,10));
      else if(arg=="--scene")scene=value;
      else zoom=std::atoi(value.c_str());
    } else {std::fprintf(stderr,"Unknown option %s\n",arg.c_str());return 1;}
  }
  if(zoom<1||zoom>4) {std::fputs("zoom must be 1..4\n",stderr);return 1;}
  if(SDL_Init(check?SDL_INIT_TIMER:SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0) {
    std::fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 1;
  }
  lv_init();
  lv_display_t* display=nullptr;
  if(check) {
    lv_tick_set_cb(clock_ms);
    display=lv_display_create(w,h);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,drawbuf.data(),nullptr,sizeof(drawbuf),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
  } else {
    display=lv_sdl_window_create(w,h);
    lv_sdl_window_set_zoom(display,static_cast<float>(zoom));
    lv_sdl_window_set_title(display,"Hatch-a-Pet (LVGL 9)");
    lv_tick_set_cb(SDL_GetTicks);
  }
  if(!display){std::fputs("LVGL display creation failed\n",stderr);return 1;}
  color_pop::AssetPack pack;
  if(!load(pack,pack_path))return 1;
  View view;const char* missing=nullptr;
  if(!view.create(lv_screen_active(),pack,&missing)) {
    std::fprintf(stderr,"Missing sprite %s\n",missing?missing:"unknown");return 1;
  }
  Model model(seed);
  auto render=[&](double dt) {
    model.tick(dt);
    view.sync(model.state(),reduced);
    if(check)sim_ms+=static_cast<uint32_t>(dt*1000+0.5);
    lv_timer_handler();
  };
  if(check) {
    render(0.016);
    if(scene!="start") {
      model.pointer_down(160,120);model.pointer_up();
      for(int i=0;i<80;++i)render(0.016);
      if(model.state().phase!=Phase::Egg) {std::fputs("intro transition failed\n",stderr);return 2;}
      if(scene=="hint")for(int i=0;i<760;++i)render(0.016);
      if(scene!="egg"&&scene!="hint") {
        const int knocks=scene=="knock"?1:model.state().egg.need;
        for(int n=0;n<knocks;++n) {
          model.pointer_down(160,130);model.pointer_up();
          for(int i=0;i<6;++i)render(0.016);
        }
        if(scene!="knock") {
          for(int i=0;i<(scene=="hatch"?30:130);++i)render(0.016);
          if(scene!="hatch") {
            if(model.state().phase!=Phase::Pet) {std::fputs("hatch transition failed\n",stderr);return 3;}
            if(scene=="choice"||scene=="yes"||scene=="no")for(int i=0;i<320;++i)render(0.016);
            if(scene=="yes"||scene=="no") {
              model.pointer_down(scene=="yes"?272:48,168);model.pointer_up();
              for(int i=0;i<75;++i)render(0.016);
            }
          }
        }
      }
    }
    if(scene!="start"&&scene!="egg"&&scene!="knock"&&scene!="pet"&&
       scene!="choice"&&scene!="hatch"&&scene!="yes"&&scene!="no"&&scene!="hint") {
      std::fprintf(stderr,"Unknown scene %s\n",scene.c_str());return 2;
    }
    if(!output.empty()&&!screenshot(output)) {std::fputs("screenshot write failed\n",stderr);return 4;}
    std::printf("host LVGL offscreen OK: seed=%u scene=%s phase=%d t=%.3f egg_x=%.3f egg_roll=%.3f pack=%u bytes sprites=%zu\n",
                seed,scene.c_str(),static_cast<int>(model.state().phase),model.state().phase_elapsed,
                model.state().egg.x,model.state().egg.roll,pack.bytes(),pack.count());
    return 0;
  }
  Audio audio;
  if(!mute)audio.open();
  bool pressed=false;
  uint32_t last=SDL_GetTicks();
  std::puts("Hatch-a-Pet: click to start, tap egg or pet; Home top left, R reduced motion, Esc quit.");
  for(;;) {
    const uint32_t now=SDL_GetTicks();
    int x=0,y=0;
    const bool down=(SDL_GetMouseState(&x,&y)&SDL_BUTTON(SDL_BUTTON_LEFT))!=0;
    if(down&&!pressed)model.pointer_down(double(x)/zoom,double(y)/zoom);
    if(!down&&pressed)model.pointer_up();
    pressed=down;
    const Uint8* keys=SDL_GetKeyboardState(nullptr);
    if(keys[SDL_SCANCODE_ESCAPE])break;
    if(keys[SDL_SCANCODE_R])reduced=true;
    render(std::min(0.05,(now-last)/1000.0));
    last=now;
    CueEvent cue;
    while(model.pop_cue(cue))audio.cue(cue);
    SDL_Delay(5);
  }
  return 0;
}
