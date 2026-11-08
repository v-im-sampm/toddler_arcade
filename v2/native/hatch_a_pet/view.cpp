#include "view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hatch_a_pet {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr uint32_t colors[] = {0x2EC4B6, 0xFFAA80, 0x5BC85B, 0x9B6BE8,
                                0x4D9BF0, 0xFFD23F, 0xFF8A2A, 0xF06BB8};
int roundi(double x) { return static_cast<int>(std::lround(x)); }
double clamp(double x, double lo, double hi) { return std::max(lo, std::min(x, hi)); }
}
View::Layer View::make(lv_obj_t* parent) {
  Layer layer;
  layer.image = lv_image_create(parent);
  lv_obj_remove_flag(layer.image, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(layer.image, LV_OBJ_FLAG_HIDDEN);
  return layer;
}
void View::hide(Layer& layer) { lv_obj_add_flag(layer.image, LV_OBJ_FLAG_HIDDEN); }
void View::show(Layer& layer, const char* name, double x, double y, double sx,
                double sy, double rotation, double opacity, uint32_t tint) {
  const auto* sprite = pack_->find(name);
  if (!sprite) { if (!missing_) missing_ = name; hide(layer); return; }
  if (layer.sprite != sprite) {
    lv_image_set_src(layer.image, &sprite->dsc);
    lv_image_set_pivot(layer.image, roundi(sprite->ox), roundi(sprite->oy));
    layer.sprite = sprite;
  }
  lv_obj_set_pos(layer.image, roundi(x-sprite->ox), roundi(y-sprite->oy));
  lv_image_set_scale_x(layer.image, std::max(1, roundi(sx*256)));
  lv_image_set_scale_y(layer.image, std::max(1, roundi(sy*256)));
  int angle=roundi(rotation*1800/pi)%3600;
  if(angle<0)angle+=3600;
  lv_image_set_rotation(layer.image,angle);
  lv_obj_set_style_image_opa(layer.image, roundi(clamp(opacity,0,1)*255), 0);
  if (sprite->dsc.header.cf == LV_COLOR_FORMAT_A8) {
    lv_obj_set_style_image_recolor(layer.image, lv_color_hex(tint), 0);
    lv_obj_set_style_image_recolor_opa(layer.image, LV_OPA_COVER, 0);
  }
  lv_obj_remove_flag(layer.image, LV_OBJ_FLAG_HIDDEN);
}
bool View::create(lv_obj_t* screen, const color_pop::AssetPack& pack, const char** missing) {
  pack_=&pack;
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(screen,0,0);
  lv_obj_set_style_bg_color(screen,lv_color_hex(0xBFE6FF),0);
  background_=make(screen);
  for(auto& cloud:clouds_)cloud=make(screen);
  for(auto& l:shelf_) l=make(screen);
  nest_back_=make(screen); shell_bot_=make(screen); bot_decal_=make(screen);
  egg_glow_=make(screen);egg_=make(screen); decal_=make(screen);
  crack_=make(screen); nest_front_=make(screen); pet_=make(screen);
  shell_top_=make(screen); top_decal_=make(screen);
  choice_glow_no_=make(screen);choice_glow_yes_=make(screen);
  choice_no_=make(screen); choice_yes_=make(screen); hand_=make(screen);
  for(auto& particle:particles_)particle.layer=make(screen);
  badge_glow_=make(screen);badge_base_=make(screen);badge_=make(screen);
  home_=make(screen); mic_=make(screen);
  veil_=lv_obj_create(screen);
  lv_obj_remove_style_all(veil_);
  lv_obj_set_size(veil_,320,240);
  lv_obj_set_style_bg_color(veil_,lv_color_hex(0x1E2245),0);
  lv_obj_set_style_bg_opa(veil_,89,0);
  lv_obj_remove_flag(veil_,LV_OBJ_FLAG_CLICKABLE);
  start_=make(screen);
  feedback_=lv_obj_create(screen);
  lv_obj_remove_style_all(feedback_);
  lv_obj_set_size(feedback_,32,32);
  lv_obj_set_style_radius(feedback_,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_border_width(feedback_,2,0);
  lv_obj_set_style_border_color(feedback_,lv_color_white(),0);
  lv_obj_remove_flag(feedback_,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(feedback_,LV_OBJ_FLAG_HIDDEN);
  show(background_,"background",0,0);
  for(auto& cloud:clouds_)show(cloud,"cloud",40,22,1,1,0,1,0xFFFFFF);
  show(nest_back_,"nest_back",160,198);
  show(nest_front_,"nest_front",160,198);
  show(home_,"home",4,4);
  show(mic_,"mic",268,4);
  if(missing) *missing=missing_;
  return !missing_;
}
void View::emit(double x,double y,int count,const char* shape,uint32_t color,double gravity) {
  auto random=[&]() {
    visual_rng_^=visual_rng_<<13;visual_rng_^=visual_rng_>>17;visual_rng_^=visual_rng_<<5;
    return (visual_rng_&0xFFFFFF)/16777216.0;
  };
  for(int n=0;n<count;++n) {
    Particle* p=nullptr;
    for(auto& candidate:particles_)if(candidate.life<=0) {p=&candidate;break;}
    if(!p) p=&particles_[n%particles_.size()];
    const double a=random()*2*pi,speed=40+random()*100;
    p->x=x;p->y=y;p->vx=std::cos(a)*speed;p->vy=std::sin(a)*speed-40;
    p->gravity=gravity;p->duration=0.4+random()*0.55;p->life=p->duration;
    p->angle=random()*2*pi;p->angular=(random()-0.5)*8;
    p->color=color;p->shape=shape;
  }
}
void View::sync(const State& s, bool reduced) {
  char id[32];
  const int c=static_cast<int>(s.egg.creature), pattern=static_cast<int>(s.egg.pattern);
  const bool egg_phase=s.phase==Phase::Intro||s.phase==Phase::Egg||
                       s.phase==Phase::PreHatch||s.phase==Phase::Roll;
  const double m=reduced?0.3:1;
  const double dt=clamp(s.elapsed-last_elapsed_,0,0.05);
  last_elapsed_=s.elapsed;
  if(s.phase==Phase::Start) {
    for(auto& p:particles_)p.life=0;
    last_hatches_=s.hatches;
  }
  for(auto& p:particles_)if(p.life>0) {
    p.life-=dt;p.vy+=p.gravity*dt;p.x+=p.vx*dt;p.y+=p.vy*dt;
    p.angle+=p.angular*dt;
  }
  if(s.hatches!=last_hatches_) {
    last_hatches_=s.hatches;
    emit(160,124,reduced?15:30,s.egg.pattern==Pattern::Stars?"particle_star":
         s.egg.pattern==Pattern::Spots?"particle_circle":"particle_rect",colors[c],140);
    emit(160,124,reduced?8:16,"particle_star",0xFFEB9C,60);
  }
  if(s.egg.cracks>last_cracks_&&
     (s.phase==Phase::Egg||s.phase==Phase::PreHatch))
    emit(160,124,reduced?3:6,"particle_circle",0xE3CBA0,300);
  last_cracks_=s.egg.cracks;
  for(auto& p:particles_) {
    if(p.life>0)show(p.layer,p.shape,p.x,p.y,1,1,p.angle,clamp(p.life/p.duration*1.6,0,1),p.color);
    else hide(p.layer);
  }
  show(clouds_[0],"cloud",std::fmod(40+s.elapsed*4*m,400)-40,22,1,1,0,1,0xFFFFFF);
  show(clouds_[1],"cloud",std::fmod(210+s.elapsed*2.6*m,400)-40,52,1,1,0,1,0xFFFFFF);
  if(egg_phase) {
    const double breath=std::sin(s.elapsed*2*pi/2.8)*0.01*m;
    const double sx=(1-s.egg.squash)*(1+breath*0.5);
    const double sy=(1+s.egg.squash)*(1+breath);
    const double x=s.egg.x+std::sin(s.egg.angle)*64*sy;
    const double y=138+s.egg.y_offset+64*(1-std::cos(s.egg.angle)*sy);
    if(s.phase==Phase::Egg&&s.hint>=1)
      show(egg_glow_,"egg_glow",x,y,sx,sy,s.egg.angle,
           0.35+0.4*(0.5+0.5*std::sin(s.elapsed*2*pi/0.9)),0xFFE066);
    else hide(egg_glow_);
    show(egg_,"egg_shell",x,y,sx,sy,s.egg.angle*m+s.egg.roll);
    std::snprintf(id,sizeof id,"egg_decal_%d",pattern);
    show(decal_,id,x,y,sx,sy,s.egg.angle*m+s.egg.roll,1,colors[c]);
    if(s.egg.cracks) {
      std::snprintf(id,sizeof id,"crack_%d",
                    std::max(1,std::min(5,roundi(double(s.egg.cracks)*5/s.egg.need))));
      show(crack_,id,x,y,sx,sy,s.egg.angle*m+s.egg.roll);
    } else hide(crack_);
  } else {hide(egg_glow_);hide(egg_);hide(decal_);hide(crack_);}
  if(s.shell.visible) {
    show(shell_bot_,"shell_bot",160,138+s.shell.bottom_y,1,1,0,
         1-clamp((s.shell.elapsed-0.5)/0.7,0,1));
    std::snprintf(id,sizeof id,"shell_bot_decal_%d",pattern);
    show(bot_decal_,id,160,138+s.shell.bottom_y,1,1,0,
         1-clamp((s.shell.elapsed-0.5)/0.7,0,1),colors[c]);
    const double tx=160+s.shell.top_x-14*std::sin(s.shell.top_rotation);
    const double ty=138+s.shell.top_y+14*(std::cos(s.shell.top_rotation)-1);
    show(shell_top_,"shell_top",tx,ty,1,1,s.shell.top_rotation,
         1-clamp((s.shell.elapsed-0.6)/0.6,0,1));
    std::snprintf(id,sizeof id,"shell_top_decal_%d",pattern);
    show(top_decal_,id,tx,ty,1,1,s.shell.top_rotation,
         1-clamp((s.shell.elapsed-0.6)/0.6,0,1),colors[c]);
  } else {hide(shell_bot_);hide(shell_top_);hide(bot_decal_);hide(top_decal_);}
  const bool pet_phase=s.phase==Phase::Hatch||s.phase==Phase::Pet||s.phase==Phase::Bye;
  if(pet_phase && (s.phase!=Phase::Hatch||s.phase_elapsed>=0.05)) {
    const char* mood="idle";
    if(s.phase==Phase::Hatch) mood=s.phase_elapsed>0.25?"happy":"open";
    if(s.pet.reaction==Reaction::Giggle) mood="happy";
    if(s.pet.reaction==Reaction::Jump) mood="open";
    if(s.pet.reaction==Reaction::Sneeze) mood=s.pet.reaction_seconds>0.45?"squint":"happy";
    if(s.phase==Phase::Bye) mood="happy";
    std::snprintf(id,sizeof id,"pet_%d_%s",c,mood);
    const double pet_sy=s.pet.scale*(1+s.pet.squash);
    show(pet_,id,s.pet.x+std::sin(s.pet.pose_rotation)*34*pet_sy,
         s.pet.y+s.pet.pose_y_offset+34*(s.pet.scale-std::cos(s.pet.pose_rotation)*pet_sy),
         s.pet.scale*(1-s.pet.squash),s.pet.scale*(1+s.pet.squash),s.pet.pose_rotation);
  } else hide(pet_);
  for(size_t i=0;i<shelf_.size();++i) {
    if(i<s.shelf.size()) {
      const auto& f=s.shelf[i];
      std::snprintf(id,sizeof id,"pet_%d_%s",static_cast<int>(f.creature),
                    f.wave_seconds>0?"happy":"idle");
      show(shelf_[i],id,f.x,f.y+std::sin(f.phase*2.1)*m,
           0.32*(1-f.squash),0.32*(1+f.squash),
           f.wave_seconds>0?std::sin(f.wave_seconds*30)*0.15:0);
    } else hide(shelf_[i]);
  }
  if(s.choice.visible||s.choice.fade_seconds>0) {
    const double t=clamp(s.choice.elapsed/0.5,0,1);
    const double scale=s.choice.visible?1+2.70158*std::pow(t-1,3)+1.70158*std::pow(t-1,2):
                       s.choice.fade_seconds/0.35;
    const double rise=(1-scale)*90;
    if(s.choice.visible&&s.choice.idle_seconds>6) {
      const double glow=0.5+0.5*std::sin(s.elapsed*2*pi/0.9);
      show(choice_glow_no_,"choice_glow",48,168+rise,scale,scale,0,0.35+0.5*glow,0xFFE066);
      show(choice_glow_yes_,"choice_glow",272,168+rise,scale,scale,0,0.35+0.5*glow,0xFFE066);
    }else{hide(choice_glow_no_);hide(choice_glow_yes_);}
    show(choice_no_,"choice_no",48,168+rise+std::sin(s.elapsed*2.1)*2*m,scale,scale);
    show(choice_yes_,"choice_yes",272,168+rise+std::sin(s.elapsed*2.1+1.7)*2*m,scale,scale);
  } else {hide(choice_glow_no_);hide(choice_glow_yes_);hide(choice_no_);hide(choice_yes_);}
  if(s.phase==Phase::Egg&&s.hint>=2)
    show(hand_,"hint_hand",190,162-std::abs(std::sin(s.elapsed*4))*6*m);
  else hide(hand_);
  if(s.phase!=Phase::Start&&s.phase!=Phase::Intro) {
    const double by=32+std::sin(s.elapsed*2.2)*1.5*m;
    if(s.phase==Phase::Egg&&s.hint>=1)
      show(badge_glow_,"badge_glow",160,by,1,1,0,
           0.4+0.4*std::sin(s.elapsed*2*pi/0.9),0xFFE066);
    else hide(badge_glow_);
    show(badge_base_,"badge_base",160,by);
    std::snprintf(id,sizeof id,"badge_%d_%s",c,
      s.badge_happy_seconds>0?"happy":(s.phase==Phase::Egg||s.phase==Phase::PreHatch||s.phase==Phase::Roll?"egg":"heart"));
    show(badge_,id,160,by);
  } else {hide(badge_glow_);hide(badge_base_);hide(badge_);}
  if(s.phase==Phase::Start) {
    lv_obj_remove_flag(veil_,LV_OBJ_FLAG_HIDDEN);
    show(start_,"start",160,120,1+std::sin(s.elapsed*3)*0.04*m,
         1+std::sin(s.elapsed*3)*0.04*m);
  } else {lv_obj_add_flag(veil_,LV_OBJ_FLAG_HIDDEN);hide(start_);}
  if(s.touch.serial!=touch_serial_) {
    touch_serial_=s.touch.serial;
    feedback_time_=s.elapsed;
  }
  const double age=s.elapsed-feedback_time_;
  if(age>=0&&age<0.5) {
    const int radius=roundi(5+35*age/0.5);
    lv_obj_set_size(feedback_,2*radius,2*radius);
    lv_obj_set_pos(feedback_,roundi(s.touch.x)-radius,roundi(s.touch.y)-radius);
    lv_obj_set_style_border_opa(feedback_,roundi(255*(1-age/0.5)),0);
    lv_obj_remove_flag(feedback_,LV_OBJ_FLAG_HIDDEN);
  } else lv_obj_add_flag(feedback_,LV_OBJ_FLAG_HIDDEN);
}
}
