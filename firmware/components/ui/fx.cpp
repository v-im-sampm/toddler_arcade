#include "little_arcade/ui/fx.hpp"

#include <cmath>
#include <cstdlib>
#ifndef ESP_PLATFORM
#include <cstdio>
#endif

#include "little_arcade/geometry.hpp"
#include "little_arcade/ui/shade.hpp"
#include "src/misc/cache/instance/lv_image_cache.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
#endif

namespace little_arcade::ui {

namespace {
#ifdef ESP_PLATFORM
constexpr const char* kLogTag = "ui_fx";
#endif
constexpr int kSpritePx = 16;  // white sprites, recoloured per particle
constexpr int kShapeCount = 6;
ArgbImage g_shapes[kShapeCount];
bool g_shapes_ready = false;

void paint_shapes() {
  if (g_shapes_ready) return;
  const float s = static_cast<float>(kSpritePx), m = s * 0.5f;
  for (int i = 0; i < kShapeCount; ++i) {
    if (!g_shapes[i].allocate(kSpritePx, kSpritePx)) return;
    Canvas c = g_shapes[i].canvas();
    switch (static_cast<FxShape>(i)) {
      case FxShape::DOT: fill_circle(c, m, m, m - 0.5f, tok::kWhite); break;
      case FxShape::STAR: {
        Path p;
        for (int k = 0; k < 10; ++k) {
          const float r = (k % 2) ? m * 0.42f : m - 0.5f, a = -1.5707963f + static_cast<float>(k) * 0.62831853f;
          p.line(m + std::cos(a) * r, m + std::sin(a) * r);
        }
        fill_path(c, p, tok::kWhite);
        break;
      }
      case FxShape::CONFETTI: fill_rect(c, 1, m - 3.5f, s - 1, m + 3.5f, tok::kWhite); break;
      case FxShape::BUBBLE:
        stroke_circle(c, m, m, m - 1.5f, 1.8f, tok::kWhite);
        fill_circle(c, m - 3, m - 3, 1.6f, tok::kWhite);
        break;
      case FxShape::HEART:
        paint(c, 0, 0, s, s, tok::kWhite, 255, [m, s](float x, float y) { return in_heart(x, y, m, m, s * 0.95f); });
        break;
      case FxShape::NOTE: {
        fill_ellipse(c, m - 2.5f, s - 4.5f, 3.6f, 2.8f, -0.4f, tok::kWhite);
        fill_rect(c, m + 0.2f, 2, m + 2.2f, s - 4.5f, tok::kWhite);
        Path flag;
        flag.move(m + 1, 2).quad(m + 7, 4, m + 5, 9).line(m + 2, 6).line(m + 1, 6);
        fill_path(c, flag, tok::kWhite);
        break;
      }
    }
  }
  g_shapes_ready = true;
}

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

void* alloc_bytes(std::size_t bytes) {
#ifdef ESP_PLATFORM
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
  return std::malloc(bytes);
#endif
}

ArgbImage g_scratch;  // 320x240 ARGB painting surface, shared by all backdrops
}  // namespace

void FxLayer::create(lv_obj_t* parent, uint32_t seed) {
  paint_shapes();
  sim_ = FxSim<kMaxParticles>(seed);
  clock_.reset();
  for (Ring& r : rings_) {
    r = Ring{};
    r.obj = plain(parent);
    lv_obj_set_style_radius(r.obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(r.obj, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
  }
  for (Slot& s : slots_) {
    s = Slot{};
    s.img = lv_image_create(parent);
    lv_obj_remove_flag(s.img, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_image_set_pivot(s.img, kSpritePx / 2, kSpritePx / 2);
    lv_obj_set_style_image_recolor_opa(s.img, LV_OPA_COVER, 0);
    lv_obj_add_flag(s.img, LV_OBJ_FLAG_HIDDEN);
  }
}

void FxLayer::clear() {
  sim_.clear();
  for (Ring& r : rings_) r.alive = false;
}

void FxLayer::ring(float x, float y, float r0, float r1, uint32_t ms, uint32_t rgb, int width) {
  Ring* pick = &rings_[0];
  for (Ring& r : rings_) {
    if (!r.alive) {
      pick = &r;
      break;
    }
    if (r.start < pick->start) pick = &r;
  }
  if (!pick->obj) return;
  pick->x = x;
  pick->y = y;
  pick->r0 = r0;
  pick->r1 = r1;
  pick->ms = ms ? ms : 1;
  pick->alive = true;
  pick->started = false;  // start time is taken on the next update()
  lv_obj_set_style_border_color(pick->obj, lv_color_hex(rgb), 0);
  lv_obj_set_style_border_width(pick->obj, width, 0);
}

void FxLayer::update(uint32_t now_ms) {
  const float dt = clock_.dt(now_ms);
  now_ = now_ms;
  sim_.step(dt);

  for (int i = 0; i < kMaxParticles; ++i) {
    const FxParticle& p = sim_.at(i);
    Slot& s = slots_[i];
    if (!s.img) continue;
    const bool show = p.alive && p.x > -16 && p.x < kScreenWidth + 16 && p.y > -16 && p.y < kScreenHeight + 16;
    if (show != s.shown) {
      s.shown = show;
      if (show) {
        lv_obj_remove_flag(s.img, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(s.img, LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (!show) continue;
    const int shape = static_cast<int>(p.shape);
    if (shape != s.shape) {
      s.shape = shape;
      if (g_shapes[shape].ready()) lv_image_set_src(s.img, g_shapes[shape].dsc());
      lv_obj_set_style_blend_mode(s.img, p.additive ? LV_BLEND_MODE_ADDITIVE : LV_BLEND_MODE_NORMAL, 0);
    }
    if (p.color != s.color) {
      s.color = p.color;
      lv_obj_set_style_image_recolor(s.img, lv_color_hex(p.color), 0);
    }
    float size = p.size;
    if (p.shape == FxShape::DOT) size *= 0.5f + 0.5f * p.life / p.max_life;  // shrink as it fades
    const int32_t scale = static_cast<int32_t>(size * 256.0f / static_cast<float>(kSpritePx));
    lv_image_set_scale(s.img, static_cast<uint32_t>(scale < 16 ? 16 : scale));
    if (p.shape == FxShape::CONFETTI || p.shape == FxShape::STAR) {
      lv_image_set_rotation(s.img, static_cast<int32_t>(std::fmod(p.rot * 57.29578f, 360.0f) * 10.0f));
    }
    lv_obj_set_pos(s.img, static_cast<int32_t>(std::lround(p.x)) - kSpritePx / 2,
                   static_cast<int32_t>(std::lround(p.y)) - kSpritePx / 2);
    lv_obj_set_style_image_opa(s.img, static_cast<lv_opa_t>(p.fade() * 255.0f), 0);
  }

  for (Ring& r : rings_) {
    if (!r.obj) continue;
    if (r.alive && !r.started) {
      r.started = true;
      r.start = now_ms;
    }
    const float t = r.alive ? static_cast<float>(now_ms - r.start) / static_cast<float>(r.ms) : 1.0f;
    if (t >= 1.0f) {
      if (r.alive || !lv_obj_has_flag(r.obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
      r.alive = false;
      continue;
    }
    lv_obj_remove_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
    const float rad = motion::lerp(r.r0, r.r1, motion::ease_out_cubic(t));
    const int32_t d = static_cast<int32_t>(rad * 2.0f);
    lv_obj_set_size(r.obj, d, d);
    lv_obj_set_pos(r.obj, static_cast<int32_t>(r.x - rad), static_cast<int32_t>(r.y - rad));
    lv_obj_set_style_border_opa(r.obj, static_cast<lv_opa_t>((1.0f - t) * 255.0f), 0);
  }
}

Canvas Backdrop::begin() {
  if (!g_scratch.allocate(kScreenWidth, kScreenHeight)) {
#ifdef ESP_PLATFORM
    ESP_LOGE(kLogTag, "Unable to allocate the shared scene painting surface");
#else
    std::fprintf(stderr, "ui_fx: unable to allocate the shared scene painting surface\n");
#endif
    return Canvas{};
  }
  return g_scratch.canvas();
}

lv_obj_t* Backdrop::show(lv_obj_t* parent) {
  if (!rgb_) {
    rgb_ = static_cast<uint16_t*>(alloc_bytes(sizeof(uint16_t) * kScreenWidth * kScreenHeight));
  }
  if (!rgb_ || !g_scratch.ready()) {
#ifdef ESP_PLATFORM
    ESP_LOGE(kLogTag, "Unable to allocate the shared RGB565 scene image");
#else
    std::fprintf(stderr, "ui_fx: unable to allocate the shared RGB565 scene image\n");
#endif
    return nullptr;
  }
  to_rgb565_dithered(g_scratch.canvas(), rgb_);
  dsc_ = lv_image_dsc_t{};
  dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
  dsc_.header.w = kScreenWidth;
  dsc_.header.h = kScreenHeight;
  dsc_.header.stride = kScreenWidth * 2;
  dsc_.data_size = kScreenWidth * kScreenHeight * 2;
  dsc_.data = reinterpret_cast<const uint8_t*>(rgb_);
  lv_image_cache_drop(&dsc_);
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_remove_flag(img, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_image_set_src(img, &dsc_);
  lv_obj_set_pos(img, 0, 0);
  return img;
}

Backdrop& scene_backdrop() {
  static Backdrop b;
  return b;
}

lv_obj_t* sprite(lv_obj_t* parent, const ArgbImage& art) {
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_remove_flag(img, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  if (art.ready()) {
    lv_image_set_src(img, art.dsc());
    lv_image_set_pivot(img, art.width() / 2, art.height() / 2);
  }
  return img;
}

void place_sprite(lv_obj_t* img, const ArgbImage& art, float cx, float cy, float sx, float sy, float rot_deg) {
  lv_obj_set_pos(img, static_cast<int32_t>(std::lround(cx - art.width() * 0.5f)),
                 static_cast<int32_t>(std::lround(cy - art.height() * 0.5f)));
  lv_image_set_scale_x(img, static_cast<uint32_t>(std::lround(sx * 256.0f)));
  lv_image_set_scale_y(img, static_cast<uint32_t>(std::lround(sy * 256.0f)));
  lv_image_set_rotation(img, static_cast<int32_t>(std::lround(rot_deg * 10.0f)));
}

}  // namespace little_arcade::ui
