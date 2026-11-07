#include "little_arcade/ui/ctx2d.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

namespace little_arcade::ui::c2d {
namespace {

// All recording math is single precision: the ESP32-S3 FPU has no double
// support, and software doubles made recording ~400x slower than on a PC.
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;
// Max distance (device px) between a flattened curve and the true curve.
constexpr float kMiterLimit = 10.0f;

void* alloc_zeroed(std::size_t bytes) {
#ifdef ESP_PLATFORM
  void* p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_calloc(1, bytes, MALLOC_CAP_8BIT);
  return p;
#else
  return std::calloc(1, bytes);
#endif
}

inline uint32_t mix(uint32_t h, uint32_t v) {
  h ^= v;
  h *= 0x85EBCA6Bu;
  h ^= h >> 13;
  h *= 0xC2B2AE35u;
  return h;
}
inline uint32_t fbits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}

Mat compose(const Mat& m, const Mat& t) {
  Mat r;
  r.a = m.a * t.a + m.c * t.b;
  r.b = m.b * t.a + m.d * t.b;
  r.c = m.a * t.c + m.c * t.d;
  r.d = m.b * t.c + m.d * t.d;
  r.e = m.a * t.e + m.c * t.f + m.e;
  r.f = m.b * t.e + m.d * t.f + m.f;
  return r;
}

bool invert(const Mat& m, Mat& out) {
  const float det = m.a * m.d - m.b * m.c;
  if (std::fabs(det) < 1e-12f) return false;
  const float r = 1.0f / det;
  const float ia = m.d * r, ib = -m.b * r, ic = -m.c * r, id = m.a * r;
  out.a = ia;
  out.b = ib;
  out.c = ic;
  out.d = id;
  out.e = -(ia * m.e + ic * m.f);
  out.f = -(ib * m.e + id * m.f);
  return true;
}

inline Pt apply(const Mat& m, float x, float y) { return Pt{m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f}; }

int segments_for(float radius_px, float sweep, float tol) {
  if (radius_px <= tol) return 1;
  const float step = 2.0f * std::acos(1.0f - tol / radius_px);
  int n = static_cast<int>(std::ceil(std::fabs(sweep) / step));
  if (n < 1) n = 1;
  if (n > 512) n = 512;
  return n;
}

uint8_t to_u8(float v) {
  if (v <= 0) return 0;
  if (v >= 255) return 255;
  return static_cast<uint8_t>(v + 0.5f);
}

}  // namespace

Color hsla(double hd, double sd, double ld, float a) {
  float h = std::fmod(static_cast<float>(hd), 360.0f);
  if (h < 0) h += 360.0f;
  const float s = static_cast<float>(sd) * 0.01f;
  const float l = static_cast<float>(ld) * 0.01f;
  const float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
  const float p = 2 * l - q;
  auto hue = [&](float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
  };
  const float hh = h * (1.0f / 360.0f);
  return Color{to_u8(hue(hh + 1.0f / 3) * 255), to_u8(hue(hh) * 255), to_u8(hue(hh - 1.0f / 3) * 255), a};
}

// ---------------------------------------------------------------- list

bool DisplayList::allocate(const Limits& limits) {
  if (pts_) return true;
  limits_ = limits;
  path_ = static_cast<Pt*>(alloc_zeroed(sizeof(Pt) * kPathMax));
  scratch_ = static_cast<Pt*>(alloc_zeroed(sizeof(Pt) * (kPathMax + 1)));
  pts_ = static_cast<Pt*>(alloc_zeroed(sizeof(Pt) * limits.points));
  contours_ = static_cast<Contour*>(alloc_zeroed(sizeof(Contour) * limits.contours));
  items_ = static_cast<Item*>(alloc_zeroed(sizeof(Item) * limits.items));
  grads_ = static_cast<Gradient*>(alloc_zeroed(sizeof(Gradient) * limits.gradients));
  if (!path_ || !scratch_ || !pts_ || !contours_ || !items_ || !grads_) {
    pts_ = nullptr;  // leaked on failure: allocation happens once at view creation
    return false;
  }
  clear();
  return true;
}

std::size_t DisplayList::bytes() const {
  return sizeof(Pt) * (kPathMax * 2 + 1) + sizeof(Pt) * limits_.points + sizeof(Contour) * limits_.contours +
         sizeof(Item) * limits_.items + sizeof(Gradient) * limits_.gradients;
}

void DisplayList::clear() {
  pt_count_ = 0;
  contour_count_ = 0;
  item_count_ = 0;
  grad_count_ = 0;
  overflows_ = 0;
}

// ---------------------------------------------------------------- context

void Context::reset() {
  list_.clear();
  st_ = State{};
  depth_ = 0;
  begin_path();
}

void Context::save() {
  if (depth_ < kStack) stack_[depth_] = st_;
  ++depth_;
}

void Context::restore() {
  if (depth_ == 0) return;
  --depth_;
  if (depth_ < kStack) st_ = stack_[depth_];
}

void Context::translate(double x, double y) {
  st_.m = compose(st_.m, Mat{1, 0, 0, 1, static_cast<float>(x), static_cast<float>(y)});
}

void Context::rotate(double rd) {
  const float r = static_cast<float>(rd);
  const float c = std::cos(r), s = std::sin(r);
  st_.m = compose(st_.m, Mat{c, s, -s, c, 0, 0});
}

void Context::scale(double sx, double sy) {
  st_.m = compose(st_.m, Mat{static_cast<float>(sx), 0, 0, static_cast<float>(sy), 0, 0});
}

void Context::set_global_alpha(double a) {
  if (a >= 0.0 && a <= 1.0) st_.alpha = static_cast<float>(a);  // canvas ignores out-of-range values
}

int Context::create_linear_gradient(double x0, double y0, double x1, double y1) {
  if (list_.grad_count_ >= list_.limits_.gradients) {
    ++list_.overflows_;
    return -1;
  }
  Gradient& g = list_.grads_[list_.grad_count_];
  g = Gradient{};
  g.radial = false;
  g.x0 = static_cast<float>(x0);
  g.y0 = static_cast<float>(y0);
  g.x1 = static_cast<float>(x1);
  g.y1 = static_cast<float>(y1);
  return static_cast<int>(list_.grad_count_++);
}

int Context::create_radial_gradient(double x0, double y0, double r0, double x1, double y1, double r1) {
  const int id = create_linear_gradient(x0, y0, x1, y1);
  if (id < 0) return id;
  Gradient& g = list_.grads_[id];
  g.radial = true;
  g.r0 = static_cast<float>(r0);
  g.r1 = static_cast<float>(r1);
  return id;
}

void Context::add_color_stop(int id, double offset, Color c) {
  if (id < 0 || static_cast<uint32_t>(id) >= list_.grad_count_) return;
  Gradient& g = list_.grads_[id];
  if (g.stop_count >= 4) return;
  g.offsets[g.stop_count] = static_cast<float>(offset);
  g.stops[g.stop_count] = c;
  ++g.stop_count;
  g.lut_ready = false;
}

Pt Context::map(double x, double y) const {
  return apply(st_.m, static_cast<float>(x), static_cast<float>(y));
}

float Context::device_scale() const {
  const float sx = std::sqrt(st_.m.a * st_.m.a + st_.m.b * st_.m.b);
  const float sy = std::sqrt(st_.m.c * st_.m.c + st_.m.d * st_.m.d);
  return sx > sy ? sx : sy;
}

void Context::begin_path() {
  path_count_ = 0;
  sub_count_ = 0;
  have_current_ = false;
}

void Context::add_device(Pt p) {
  if (path_count_ >= DisplayList::kPathMax || sub_count_ == 0) {
    if (path_count_ >= DisplayList::kPathMax) ++list_.overflows_;
    return;
  }
  list_.path_[path_count_++] = p;
  ++subs_[sub_count_ - 1].count;
}

void Context::move_to(double x, double y) {
  if (sub_count_ >= kSubMax) {
    ++list_.overflows_;
    return;
  }
  subs_[sub_count_++] = Sub{path_count_, 0, false};
  add_device(map(x, y));
  have_current_ = true;
}

void Context::line_to(double x, double y) {
  if (!have_current_) {
    move_to(x, y);
    return;
  }
  add_device(map(x, y));
}

void Context::close_path() {
  if (sub_count_ == 0) return;
  Sub& s = subs_[sub_count_ - 1];
  if (s.count == 0) return;
  s.closed = true;
  const Pt start = list_.path_[s.first];
  // A new subpath begins at the closed subpath's first point.
  if (sub_count_ < kSubMax) {
    subs_[sub_count_++] = Sub{path_count_, 0, false};
    add_device(start);
  }
}

void Context::quadratic_curve_to(double cx, double cy, double x, double y) {
  if (!have_current_) move_to(cx, cy);
  const Pt p0 = list_.path_[path_count_ - 1];
  const Pt p1 = map(cx, cy), p2 = map(x, y);
  const float ddx = p0.x - 2.0f * p1.x + p2.x, ddy = p0.y - 2.0f * p1.y + p2.y;
  const float dd = std::sqrt(ddx * ddx + ddy * ddy);
  int n = static_cast<int>(std::ceil(std::sqrt(0.25f * dd / flat_tol_)));
  if (n < 1) n = 1;
  if (n > 128) n = 128;
  const float inv_n = 1.0f / static_cast<float>(n);
  for (int i = 1; i <= n; ++i) {
    const float t = static_cast<float>(i) * inv_n, u = 1.0f - t;
    add_device(Pt{u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x, u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y});
  }
}

void Context::bezier_curve_to(double c1x, double c1y, double c2x, double c2y, double x, double y) {
  if (!have_current_) move_to(c1x, c1y);
  const Pt p0 = list_.path_[path_count_ - 1];
  const Pt p1 = map(c1x, c1y), p2 = map(c2x, c2y), p3 = map(x, y);
  const float ax = p0.x - 2.0f * p1.x + p2.x, ay = p0.y - 2.0f * p1.y + p2.y;
  const float bx = p1.x - 2.0f * p2.x + p3.x, by = p1.y - 2.0f * p2.y + p3.y;
  const float dd = std::sqrt(std::fmax(ax * ax + ay * ay, bx * bx + by * by));
  int n = static_cast<int>(std::ceil(std::sqrt(0.75f * dd / flat_tol_)));
  if (n < 1) n = 1;
  if (n > 128) n = 128;
  const float inv_n = 1.0f / static_cast<float>(n);
  for (int i = 1; i <= n; ++i) {
    const float t = static_cast<float>(i) * inv_n, u = 1.0f - t;
    const float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    add_device(Pt{w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x, w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y});
  }
}

void Context::arc_points(float cx, float cy, float rx, float ry, float rot, float a0, float sweep) {
  const int n = segments_for((rx > ry ? rx : ry) * device_scale(), sweep, flat_tol_);
  const float cr = std::cos(rot), sr = std::sin(rot);
  // Incremental rotation (one cos/sin per arc, not per point).
  const float step = sweep / static_cast<float>(n), cs = std::cos(step), sn = std::sin(step);
  float c = std::cos(a0), s = std::sin(a0);
  for (int i = 0; i <= n; ++i) {
    if (i == n) {
      c = std::cos(a0 + sweep);
      s = std::sin(a0 + sweep);
    }
    const float ux = rx * c, uy = ry * s;
    const float x = cx + ux * cr - uy * sr, y = cy + ux * sr + uy * cr;
    if (i == 0) {
      line_to(x, y);  // canvas: straight line from the current point to the arc start
    } else {
      add_device(apply(st_.m, x, y));
    }
    const float nc = c * cs - s * sn;
    s = s * cs + c * sn;
    c = nc;
  }
}

namespace {
float canvas_sweep(double a0d, double a1d, bool ccw) {
  // The difference is formed in double so large absolute angles keep their
  // precision; everything after that is float.
  const float d = static_cast<float>(ccw ? a0d - a1d : a1d - a0d);
  if (d >= kTau) return ccw ? -kTau : kTau;
  float s = std::fmod(d, kTau);
  if (s < 0) s += kTau;
  return ccw ? -s : s;
}
}  // namespace

void Context::arc(double x, double y, double r, double a0, double a1, bool ccw) {
  if (r < 0) return;
  const float fr = static_cast<float>(r);
  arc_points(static_cast<float>(x), static_cast<float>(y), fr, fr, 0, static_cast<float>(a0),
             canvas_sweep(a0, a1, ccw));
}

void Context::ellipse(double x, double y, double rx, double ry, double rot, double a0, double a1, bool ccw) {
  if (rx < 0 || ry < 0) return;
  arc_points(static_cast<float>(x), static_cast<float>(y), static_cast<float>(rx), static_cast<float>(ry),
             static_cast<float>(rot), static_cast<float>(a0), canvas_sweep(a0, a1, ccw));
}

void Context::arc_to(double x1, double y1, double x2, double y2, double r) {
  if (!have_current_) move_to(x1, y1);
  Mat inv;
  if (!invert(st_.m, inv)) return;
  const Pt d0 = list_.path_[path_count_ - 1];
  const Pt u0 = apply(inv, d0.x, d0.y);
  const double x0 = u0.x, y0 = u0.y;
  double v1x = x0 - x1, v1y = y0 - y1, v2x = x2 - x1, v2y = y2 - y1;
  const double l1 = std::sqrt(v1x * v1x + v1y * v1y), l2 = std::sqrt(v2x * v2x + v2y * v2y);
  const double cross = v1x * v2y - v1y * v2x;
  if (l1 < 1e-9 || l2 < 1e-9 || r == 0 || std::fabs(cross) < 1e-9 * l1 * l2) {
    line_to(x1, y1);
    return;
  }
  v1x /= l1;
  v1y /= l1;
  v2x /= l2;
  v2y /= l2;
  const double cosang = v1x * v2x + v1y * v2y;
  const double ang = std::acos(cosang < -1 ? -1 : (cosang > 1 ? 1 : cosang));
  const double dist = r / std::tan(ang / 2);
  const double t1x = x1 + v1x * dist, t1y = y1 + v1y * dist;
  const double t2x = x1 + v2x * dist, t2y = y1 + v2y * dist;
  double bx = v1x + v2x, by = v1y + v2y;
  const double bl = std::sqrt(bx * bx + by * by);
  bx /= bl;
  by /= bl;
  const double cd = r / std::sin(ang / 2);
  const double cx = x1 + bx * cd, cy = y1 + by * cd;
  const double as = std::atan2(t1y - cy, t1x - cx), ae = std::atan2(t2y - cy, t2x - cx);
  double sweep = ae - as;
  while (sweep > 3.14159265358979323846) sweep -= 6.28318530717958647692;
  while (sweep < -3.14159265358979323846) sweep += 6.28318530717958647692;
  arc_points(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(r), static_cast<float>(r), 0,
             static_cast<float>(as), static_cast<float>(sweep));
}

bool Context::begin_item() {
  item_failed_ = false;
  item_open_ = true;
  if (list_.item_count_ >= list_.limits_.items) {
    ++list_.overflows_;
    item_failed_ = true;
  }
  item_first_contour_ = list_.contour_count_;
  item_first_pt_ = list_.pt_count_;
  bx0_ = by0_ = 1e9f;
  bx1_ = by1_ = -1e9f;
  return !item_failed_;
}

void Context::begin_contour() { contour_first_pt_ = list_.pt_count_; }

void Context::emit(Pt p) {
  if (item_failed_) return;
  if (list_.pt_count_ >= list_.limits_.points) {
    ++list_.overflows_;
    item_failed_ = true;
    return;
  }
  list_.pts_[list_.pt_count_++] = p;
  if (p.x < bx0_) bx0_ = p.x;
  if (p.x > bx1_) bx1_ = p.x;
  if (p.y < by0_) by0_ = p.y;
  if (p.y > by1_) by1_ = p.y;
}

void Context::end_contour() {
  if (item_failed_) return;
  const uint32_t count = list_.pt_count_ - contour_first_pt_;
  if (count < 3) {
    list_.pt_count_ = contour_first_pt_;
    return;
  }
  if (list_.contour_count_ >= list_.limits_.contours) {
    ++list_.overflows_;
    item_failed_ = true;
    return;
  }
  list_.contours_[list_.contour_count_++] = Contour{contour_first_pt_, count};
}

void Context::end_item(const Paint& paint) {
  item_open_ = false;
  const uint32_t contours = list_.contour_count_ - item_first_contour_;
  auto rollback = [&] {
    list_.contour_count_ = item_first_contour_;
    list_.pt_count_ = item_first_pt_;
  };
  if (item_failed_ || contours == 0) {
    rollback();
    return;
  }
  Item& it = list_.items_[list_.item_count_];
  it = Item{};
  it.first_contour = item_first_contour_;
  it.contour_count = contours;
  auto clampi = [](float v) -> int16_t {
    if (v < -4000) return -4000;
    if (v > 4000) return 4000;
    return static_cast<int16_t>(v);
  };
  it.x0 = clampi(std::floor(bx0_));
  it.y0 = clampi(std::floor(by0_));
  it.x1 = clampi(std::floor(bx1_) + 2);
  it.y1 = clampi(std::ceil(by1_) + 1);
  if (it.x1 <= 0 || it.y1 <= 0) {
    rollback();
    return;
  }
  it.paint = paint;
  it.op = st_.op;
  it.alpha = st_.alpha * (paint.grad >= 0 ? 1.0f : paint.color.a);
  uint32_t h = 0x811C9DC5u;
  if (paint.grad >= 0 && static_cast<uint32_t>(paint.grad) < list_.grad_count_) {
    Gradient& g = list_.grads_[paint.grad];
    if (!g.lut_ready) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(i) / 255.0f;
        Color c0 = g.stops[0], c1 = g.stops[0];
        float f = 0;
        if (g.stop_count == 0) {
          c0 = c1 = Color{0, 0, 0, 0};
        } else if (t <= g.offsets[0]) {
          c0 = c1 = g.stops[0];
        } else if (t >= g.offsets[g.stop_count - 1]) {
          c0 = c1 = g.stops[g.stop_count - 1];
        } else {
          for (int k = 0; k + 1 < g.stop_count; ++k) {
            if (t >= g.offsets[k] && t <= g.offsets[k + 1]) {
              c0 = g.stops[k];
              c1 = g.stops[k + 1];
              const float span = g.offsets[k + 1] - g.offsets[k];
              f = span > 0 ? (t - g.offsets[k]) / span : 0;
              break;
            }
          }
        }
        const float a = c0.a + (c1.a - c0.a) * f;
        g.lut[i] = (static_cast<uint32_t>(to_u8(a * 255)) << 24) |
                   (static_cast<uint32_t>(to_u8(c0.r + (c1.r - c0.r) * f)) << 16) |
                   (static_cast<uint32_t>(to_u8(c0.g + (c1.g - c0.g) * f)) << 8) |
                   static_cast<uint32_t>(to_u8(c0.b + (c1.b - c0.b) * f));
      }
      uint32_t gh = g.radial ? 7u : 3u;
      gh = mix(gh, fbits(g.x0));
      gh = mix(gh, fbits(g.y0));
      gh = mix(gh, fbits(g.r0));
      gh = mix(gh, fbits(g.x1));
      gh = mix(gh, fbits(g.y1));
      gh = mix(gh, fbits(g.r1));
      for (int i = 0; i < 256; i += 15) gh = mix(gh, g.lut[i]);
      g.hash = gh;
      g.lut_ready = true;
    }
    invert(st_.m, it.inv);
    h = mix(h, g.hash);
    h = mix(h, fbits(it.inv.a));
    h = mix(h, fbits(it.inv.b));
    h = mix(h, fbits(it.inv.c));
    h = mix(h, fbits(it.inv.d));
    h = mix(h, fbits(it.inv.e));
    h = mix(h, fbits(it.inv.f));
  } else {
    it.paint.grad = -1;
    h = mix(h, (static_cast<uint32_t>(paint.color.r) << 16) | (static_cast<uint32_t>(paint.color.g) << 8) |
                   paint.color.b);
  }
  h = mix(h, fbits(it.alpha));
  h = mix(h, static_cast<uint32_t>(it.op));
  for (uint32_t c = 0; c < contours; ++c) {
    const Contour& k = list_.contours_[it.first_contour + c];
    h = mix(h, k.count);
    for (uint32_t i = 0; i < k.count; ++i) {
      h = mix(h, fbits(list_.pts_[k.first + i].x));
      h = mix(h, fbits(list_.pts_[k.first + i].y));
    }
  }
  it.hash = h;
  ++list_.item_count_;
}

void Context::fill() {
  if (!begin_item()) {
    end_item(st_.fill);
    return;
  }
  for (uint32_t s = 0; s < sub_count_; ++s) {
    begin_contour();
    for (uint32_t i = 0; i < subs_[s].count; ++i) emit(list_.path_[subs_[s].first + i]);
    end_contour();
  }
  end_item(st_.fill);
}

void Context::fill_rect(double x, double y, double w, double h) {
  // fillRect does not touch the current path.
  const Pt a = map(x, y), b = map(x + w, y), c = map(x + w, y + h), d = map(x, y + h);
  begin_item();
  begin_contour();
  emit(a);
  emit(b);
  emit(c);
  emit(d);
  end_contour();
  end_item(st_.fill);
}

namespace {
float signed_area(const Pt* p, uint32_t n) {
  float a = 0;
  Pt u = p[n - 1];
  for (uint32_t i = 0; i < n; ++i) {
    const Pt& v = p[i];
    a += u.x * v.y - v.x * u.y;
    u = v;
  }
  return a;
}
inline float det_sign(const Mat& m) { return (m.a * m.d - m.b * m.c) < 0 ? -1.0f : 1.0f; }
}  // namespace

void Context::emit_polygon_user(const Pt* user, uint32_t n, const Mat& m) {
  // Every stroke piece is emitted with the same (positive) device orientation,
  // so overlapping pieces union under the |winding| clamp instead of cancelling.
  const bool reverse = signed_area(user, n) * det_sign(m) < 0;
  begin_contour();
  for (uint32_t i = 0; i < n; ++i) {
    const Pt& p = user[reverse ? n - 1 - i : i];
    emit(apply(m, p.x, p.y));
  }
  end_contour();
}

void Context::emit_circle_user(Pt c, float r, const Mat& m, float dev_scale) {
  int n = segments_for(r * dev_scale, kTau, flat_tol_);
  if (n < 8) n = 8;
  const bool reverse = det_sign(m) < 0;
  const float step = (reverse ? -kTau : kTau) / static_cast<float>(n);
  const float cs = std::cos(step), sn = std::sin(step);
  float co = 1.0f, si = 0.0f;
  begin_contour();
  for (int i = 0; i < n; ++i) {
    emit(apply(m, c.x + r * co, c.y + r * si));
    const float nc = co * cs - si * sn;
    si = si * cs + co * sn;
    co = nc;
  }
  end_contour();
}

void Context::stroke_sub(const Pt* p, uint32_t n, bool closed, const Mat& m, float dev_scale) {
  const float hw = st_.line_width * 0.5f;
  if (n < 2) return;
  if (n == 2) closed = false;
  const uint32_t segs = closed ? n : n - 1;
  auto dir = [&](uint32_t i, float& ux, float& uy, float& len) {
    const Pt& a = p[i % n];
    const Pt& b = p[(i + 1) % n];
    ux = b.x - a.x;
    uy = b.y - a.y;
    len = std::sqrt(ux * ux + uy * uy);
    ux /= len;
    uy /= len;
  };

  // Fast path: a smooth polyline (every turn small and the inner offset not
  // folding) becomes one offset polygon (or an outer/inner ring pair).
  bool smooth = true;
  {
    const uint32_t first_joint = closed ? 0 : 1;
    const uint32_t last_joint = closed ? n : n - 1;
    for (uint32_t j = first_joint; j < last_joint && smooth; ++j) {
      float u0x, u0y, l0, u1x, u1y, l1;
      dir((j + n - 1) % n, u0x, u0y, l0);
      dir(j, u1x, u1y, l1);
      const float dot = u0x * u1x + u0y * u1y;
      const float cross = u0x * u1y - u0y * u1x;
      if (dot < 0.8253f) smooth = false;  // turn > ~0.6 rad
      else if (hw * std::fabs(cross) / (1.0f + dot) >= 0.5f * (l0 < l1 ? l0 : l1)) smooth = false;
    }
  }

  if (smooth) {
    // side = +1 left offset, -1 right offset.
    auto offset = [&](uint32_t i, float side) -> Pt {
      float nx, ny;
      if (!closed && (i == 0 || i == n - 1)) {
        float ux, uy, l;
        dir(i == 0 ? 0 : n - 2, ux, uy, l);
        nx = -uy;
        ny = ux;
      } else {
        float u0x, u0y, l0, u1x, u1y, l1;
        dir((i + n - 1) % n, u0x, u0y, l0);
        dir(i, u1x, u1y, l1);
        const float dot = u0x * u1x + u0y * u1y;
        const float k = 1.0f / (1.0f + dot);
        nx = (-u0y - u1y) * k;
        ny = (u0x + u1x) * k;
      }
      return Pt{p[i].x + side * nx * hw, p[i].y + side * ny * hw};
    };
    const float sgn = det_sign(m);
    if (!closed) {
      float area = 0;
      Pt prev = offset(0, -1);  // last vertex of the sequence below
      for (uint32_t k = 0; k < 2 * n; ++k) {
        const Pt cur = k < n ? offset(k, 1) : offset(2 * n - 1 - k, -1);
        area += prev.x * cur.y - cur.x * prev.y;
        prev = cur;
      }
      const bool reverse = area * sgn < 0;
      begin_contour();
      for (uint32_t kk = 0; kk < 2 * n; ++kk) {
        const uint32_t k = reverse ? 2 * n - 1 - kk : kk;
        const Pt q = k < n ? offset(k, 1) : offset(2 * n - 1 - k, -1);
        emit(apply(m, q.x, q.y));
      }
      end_contour();
    } else {
      float al = 0, ar = 0;
      Pt pl = offset(n - 1, 1), pr = offset(n - 1, -1);
      for (uint32_t k = 0; k < n; ++k) {
        const Pt cl = offset(k, 1), cr = offset(k, -1);
        al += pl.x * cl.y - cl.x * pl.y;
        ar += pr.x * cr.y - cr.x * pr.y;
        pl = cl;
        pr = cr;
      }
      // Outer ring keeps positive device orientation; inner ring is opposite.
      const float outer = std::fabs(al) >= std::fabs(ar) ? al : ar;
      const bool outer_is_left = std::fabs(al) >= std::fabs(ar);
      const bool forward = outer * sgn > 0;
      for (int ring = 0; ring < 2; ++ring) {
        const bool is_outer = ring == 0;
        const float side = (is_outer == outer_is_left) ? 1.0f : -1.0f;
        const bool fwd = is_outer ? forward : !forward;
        begin_contour();
        for (uint32_t kk = 0; kk < n; ++kk) {
          const Pt q = offset(fwd ? kk : n - 1 - kk, side);
          emit(apply(m, q.x, q.y));
        }
        end_contour();
      }
    }
    if (!closed && st_.cap == Cap::ROUND) {
      emit_circle_user(p[0], hw, m, dev_scale);
      emit_circle_user(p[n - 1], hw, m, dev_scale);
    }
    return;
  }

  // General path: one quad per segment, a join piece per corner, caps.
  for (uint32_t i = 0; i < segs; ++i) {
    float ux, uy, l;
    dir(i, ux, uy, l);
    const Pt& a = p[i % n];
    const Pt& b = p[(i + 1) % n];
    const float nx = -uy * hw, ny = ux * hw;
    const Pt quad[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
    emit_polygon_user(quad, 4, m);
  }
  const uint32_t first_joint = closed ? 0 : 1;
  const uint32_t last_joint = closed ? n : n - 1;
  for (uint32_t j = first_joint; j < last_joint; ++j) {
    const Pt& c = p[j];
    if (st_.join == Join::ROUND) {
      emit_circle_user(c, hw, m, dev_scale);
      continue;
    }
    float u0x, u0y, l0, u1x, u1y, l1;
    dir((j + n - 1) % n, u0x, u0y, l0);
    dir(j, u1x, u1y, l1);
    const float cross = u0x * u1y - u0y * u1x;
    const float dot = u0x * u1x + u0y * u1y;
    if (std::fabs(cross) < 1e-6f) continue;  // straight or a full reversal (no outer corner)
    const float s = cross > 0 ? -1.0f : 1.0f;
    const Pt a{c.x + s * -u0y * hw, c.y + s * u0x * hw};
    const Pt b{c.x + s * -u1y * hw, c.y + s * u1x * hw};
    const float cos_half = std::sqrt((1.0f + dot) * 0.5f);
    if (st_.join == Join::MITER && cos_half > 1e-6f && 1.0f / cos_half <= kMiterLimit) {
      const float k = hw / (1.0f + dot);
      const Pt tip{c.x + s * (-u0y - u1y) * k, c.y + s * (u0x + u1x) * k};
      const Pt poly[4] = {c, a, tip, b};
      emit_polygon_user(poly, 4, m);
    } else {
      const Pt poly[3] = {c, a, b};
      emit_polygon_user(poly, 3, m);
    }
  }
  if (!closed && st_.cap == Cap::ROUND) {
    emit_circle_user(p[0], hw, m, dev_scale);
    emit_circle_user(p[n - 1], hw, m, dev_scale);
  }
}

void Context::stroke() {
  Mat inv;
  if (!invert(st_.m, inv)) return;
  const Mat m = st_.m;
  const float ds = device_scale();
  begin_item();
  for (uint32_t s = 0; s < sub_count_ && !item_failed_; ++s) {
    const Sub& sub = subs_[s];
    // Back to user space (strokes are shaped by the transform at stroke time),
    // dropping repeated points.
    uint32_t n = 0;
    Pt* u = list_.scratch_;
    for (uint32_t i = 0; i < sub.count; ++i) {
      const Pt d = list_.path_[sub.first + i];
      const Pt q = apply(inv, d.x, d.y);
      if (n > 0 && std::fabs(q.x - u[n - 1].x) < 1e-4f && std::fabs(q.y - u[n - 1].y) < 1e-4f) continue;
      u[n++] = q;
    }
    if (sub.closed && n > 2 && std::fabs(u[0].x - u[n - 1].x) < 1e-4f && std::fabs(u[0].y - u[n - 1].y) < 1e-4f) --n;
    stroke_sub(u, n, sub.closed, m, ds);
  }
  end_item(st_.stroke);
}

// ---------------------------------------------------------------- render

namespace {

constexpr int kStride = kTile + 2;

// Signed-area coverage accumulation (after font-rs). Points are tile-local,
// x already clamped to [0, tw], y clipped to [0, th].
void accumulate(float* acc, float x0, float y0, float x1, float y1, int th) {
  if (y0 == y1) return;
  float dir = 1.0f;
  if (y0 > y1) {
    dir = -1.0f;
    float t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  const float dxdy = (x1 - x0) / (y1 - y0);
  float x = x0;
  const int ys = static_cast<int>(y0);
  int ye = static_cast<int>(y1);
  if (static_cast<float>(ye) < y1) ++ye;
  if (ye > th) ye = th;
  for (int y = ys; y < ye; ++y) {
    float* line = acc + y * kStride;
    const float top = y > y0 ? static_cast<float>(y) : y0;
    const float bot = static_cast<float>(y + 1) < y1 ? static_cast<float>(y + 1) : y1;
    const float dy = bot - top;
    const float xnext = x + dxdy * dy;
    const float d = dy * dir;
    const float xa = x < xnext ? x : xnext;
    const float xb = x < xnext ? xnext : x;
    // x is clamped to [0, tw], so truncation is floor.
    const int xai = static_cast<int>(xa);
    const float xa_floor = static_cast<float>(xai);
    int xbi = static_cast<int>(xb);
    if (static_cast<float>(xbi) < xb) ++xbi;
    const float xb_ceil = static_cast<float>(xbi);
    if (xbi <= xai + 1) {
      const float xmf = 0.5f * (x + xnext) - xa_floor;
      line[xai] += d - d * xmf;
      line[xai + 1] += d * xmf;
    } else {
      const float s = 1.0f / (xb - xa);
      const float xaf = xa - xa_floor;
      const float a0 = 0.5f * s * (1.0f - xaf) * (1.0f - xaf);
      const float xbf = xb - xb_ceil + 1.0f;
      const float am = 0.5f * s * xbf * xbf;
      line[xai] += d * a0;
      if (xbi == xai + 2) {
        line[xai + 1] += d * (1.0f - a0 - am);
      } else {
        const float a1 = s * (1.5f - xaf);
        line[xai + 1] += d * (a1 - a0);
        for (int xi = xai + 2; xi < xbi - 1; ++xi) line[xi] += d * s;
        const float a2 = a1 + static_cast<float>(xbi - xai - 3) * s;
        line[xbi - 1] += d * (1.0f - a2 - am);
      }
      line[xbi] += d * am;
    }
    x = xnext;
  }
}

void add_edge(float* acc, float x0, float y0, float x1, float y1, int tw, int th) {
  if (y0 == y1) return;
  if ((y0 <= 0 && y1 <= 0) || (y0 >= th && y1 >= th)) return;
  // Entirely right of the tile: only touches the unused carry column.
  if (x0 >= tw && x1 >= tw) return;
  // Clip to the tile's rows.
  const float fth = static_cast<float>(th);
  if (y0 < 0 || y1 < 0 || y0 > fth || y1 > fth) {
    const float dxdy = (x1 - x0) / (y1 - y0);
    if (y0 < 0) { x0 += (0 - y0) * dxdy; y0 = 0; }
    if (y1 < 0) { x1 += (0 - y1) * dxdy; y1 = 0; }
    if (y0 > fth) { x0 += (fth - y0) * dxdy; y0 = fth; }
    if (y1 > fth) { x1 += (fth - y1) * dxdy; y1 = fth; }
  }
  // Split where the edge crosses the tile's left/right columns; outside parts
  // collapse onto the boundary (left: full coverage carry, right: unused).
  const float ftw = static_cast<float>(tw);
  float xs[4] = {x0, 0, 0, x1};
  float ys[4] = {y0, 0, 0, y1};
  int k = 1;
  const float bounds[2] = {0.0f, ftw};
  float ts[2];
  int nt = 0;
  for (float bx : bounds) {
    if ((x0 < bx && x1 > bx) || (x0 > bx && x1 < bx)) ts[nt++] = (bx - x0) / (x1 - x0);
  }
  if (nt == 2 && ts[0] > ts[1]) {
    const float t = ts[0];
    ts[0] = ts[1];
    ts[1] = t;
  }
  for (int i = 0; i < nt; ++i) {
    xs[k] = x0 + (x1 - x0) * ts[i];
    ys[k] = y0 + (y1 - y0) * ts[i];
    ++k;
  }
  xs[k] = x1;
  ys[k] = y1;
  for (int i = 0; i < k; ++i) {
    float ax = xs[i], bx = xs[i + 1];
    ax = ax < 0 ? 0 : (ax > ftw ? ftw : ax);
    bx = bx < 0 ? 0 : (bx > ftw ? ftw : bx);
    accumulate(acc, ax, ys[i], bx, ys[i + 1], th);
  }
}

inline uint32_t div255(uint32_t x) {
  x += 128;
  return (x + (x >> 8)) >> 8;
}

constexpr uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Per-thread raster scratch (internal RAM on the device): [0] caller, [1] worker.
struct Scratch {
  float acc[kTile * kStride];
  uint32_t tile[kTile * kTile];
};
Scratch g_scratch[2];

constexpr int kMaxTiles = TileCache::kCols * TileCache::kRows;

struct TileQueue {
  const DisplayList* list;
  const Target* target;
  const uint16_t* todo;  // dirty tile indices (row * cols + col)
  int count;
  int cols;
  std::atomic<int> next;
};

struct TileJob {
  TileQueue* queue;
  Scratch* scratch;
  int done;
};

}  // namespace

uint16_t to565(uint32_t xrgb, int x, int y) {
  // floor(v / 255) == (v + 1 + (v >> 8)) >> 8 for v < 65535 (avoids 3 divides per pixel).
  auto q = [](uint32_t v) { return (v + 1u + (v >> 8)) >> 8; };
  const uint32_t d = (static_cast<uint32_t>(kBayer[y & 3][x & 3]) * 255u + 8u) / 16u;
  const uint32_t r = ((xrgb >> 16) & 0xFF) * 31u + d;
  const uint32_t g = ((xrgb >> 8) & 0xFF) * 63u + d;
  const uint32_t b = (xrgb & 0xFF) * 31u + d;
  return static_cast<uint16_t>((q(r) << 11) | (q(g) << 5) | q(b));
}

void Renderer::raster_item(const DisplayList& list, const Item& it, int tx, int ty, int tw, int th, uint32_t* tile,
                           float* acc) {
  int lx0 = it.x0 - tx, lx1 = it.x1 - tx, ly0 = it.y0 - ty, ly1 = it.y1 - ty;
  if (lx0 < 0) lx0 = 0;
  if (ly0 < 0) ly0 = 0;
  if (lx1 > tw) lx1 = tw;
  if (ly1 > th) ly1 = th;
  if (lx0 >= lx1 || ly0 >= ly1) return;
  int cx1 = lx1 + 2;
  if (cx1 > kStride) cx1 = kStride;
  for (int y = ly0; y < ly1; ++y) std::memset(acc + y * kStride + lx0, 0, sizeof(float) * (cx1 - lx0));

  const float ox = static_cast<float>(tx), oy = static_cast<float>(ty);
  for (uint32_t c = 0; c < it.contour_count; ++c) {
    const Contour& k = list.contours_[it.first_contour + c];
    const Pt* p = list.pts_ + k.first;
    Pt prev = p[k.count - 1];
    for (uint32_t i = 0; i < k.count; ++i) {
      const Pt cur = p[i];
      add_edge(acc, prev.x - ox, prev.y - oy, cur.x - ox, cur.y - oy, tw, th);
      prev = cur;
    }
  }

  const bool grad = it.paint.grad >= 0;
  const Gradient* g = grad ? &list.grads_[it.paint.grad] : nullptr;
  const uint32_t solid = (static_cast<uint32_t>(it.paint.color.r) << 16) |
                         (static_cast<uint32_t>(it.paint.color.g) << 8) | it.paint.color.b;
  const bool lighter = it.op == Op::LIGHTER;
  float gdx = 0, gdy = 0, glen2 = 1, ga = 0, gdr = 0, inv_glen2 = 1, inv_ga = 0;
  if (g) {
    gdx = g->x1 - g->x0;
    gdy = g->y1 - g->y0;
    gdr = g->r1 - g->r0;
    glen2 = gdx * gdx + gdy * gdy;
    ga = glen2 - gdr * gdr;
    if (!g->radial && glen2 == 0) return;
    inv_glen2 = glen2 != 0 ? 1.0f / glen2 : 0.0f;
    inv_ga = ga != 0 ? 1.0f / ga : 0.0f;
  }
  const uint32_t solid_a8 = static_cast<uint32_t>(it.alpha * 255.0f + 0.5f);
  const bool opaque_solid = !g && !lighter && solid_a8 >= 255;
  for (int y = ly0; y < ly1; ++y) {
    const float* line = acc + y * kStride;
    uint32_t* row = tile + y * kTile;
    float sum = 0;
    for (int x = lx0; x < lx1; ++x) {
      sum += line[x];
      float cov = std::fabs(sum);
      if (cov > 1.0f) cov = 1.0f;
      if (cov < 0.001f) continue;
      if (opaque_solid && cov >= 0.999f) {
        row[x] = solid;
        continue;
      }
      uint32_t src = solid;
      float alpha = cov * it.alpha;
      if (g) {
        const float px = ox + static_cast<float>(x) + 0.5f, py = oy + static_cast<float>(y) + 0.5f;
        const float u = it.inv.a * px + it.inv.c * py + it.inv.e;
        const float v = it.inv.b * px + it.inv.d * py + it.inv.f;
        float t;
        if (!g->radial) {
          t = ((u - g->x0) * gdx + (v - g->y0) * gdy) * inv_glen2;
        } else {
          const float pdx = u - g->x0, pdy = v - g->y0;
          const float b = pdx * gdx + pdy * gdy + g->r0 * gdr;
          const float cc = pdx * pdx + pdy * pdy - g->r0 * g->r0;
          if (std::fabs(ga) < 1e-6f) {
            if (b == 0) continue;
            t = cc / (2.0f * b);
          } else {
            const float disc = b * b - ga * cc;
            if (disc < 0) continue;
            const float sq = std::sqrt(disc);
            const float t1 = (b + sq) * inv_ga, t2 = (b - sq) * inv_ga;
            const float hi = t1 > t2 ? t1 : t2, lo = t1 > t2 ? t2 : t1;
            if (g->r0 + hi * gdr >= 0) t = hi;
            else if (g->r0 + lo * gdr >= 0) t = lo;
            else continue;
          }
        }
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        const uint32_t c = g->lut[static_cast<int>(t * 255.0f + 0.5f)];
        src = c & 0xFFFFFF;
        alpha *= static_cast<float>(c >> 24) * (1.0f / 255.0f);
      }
      const uint32_t a8 = static_cast<uint32_t>(alpha * 255.0f + 0.5f);
      if (a8 == 0) continue;
      const uint32_t d = row[x];
      const uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
      const uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
      uint32_t r, gg, b;
      if (lighter) {
        r = dr + div255(sr * a8);
        gg = dg + div255(sg * a8);
        b = db + div255(sb * a8);
        if (r > 255) r = 255;
        if (gg > 255) gg = 255;
        if (b > 255) b = 255;
      } else {
        const uint32_t ia = 255 - a8;
        r = div255(sr * a8 + dr * ia);
        gg = div255(sg * a8 + dg * ia);
        b = div255(sb * a8 + db * ia);
      }
      row[x] = (r << 16) | (gg << 8) | b;
    }
  }
}

void Renderer::draw_tile(const DisplayList& list, const Target& target, int tx, int ty, int tw, int th,
                         uint32_t* tile, float* acc) {
  for (int y = 0; y < th; ++y) {
    uint32_t* dst = tile + y * kTile;
    if (target.background) {
      std::memcpy(dst, target.background + (ty + y) * target.w + tx, sizeof(uint32_t) * tw);
    } else {
      std::memset(dst, 0, sizeof(uint32_t) * tw);
    }
  }
  for (uint32_t i = 0; i < list.item_count_; ++i) {
    const Item& it = list.items_[i];
    if (it.x1 <= tx || it.x0 >= tx + tw || it.y1 <= ty || it.y0 >= ty + th) continue;
    raster_item(list, it, tx, ty, tw, th, tile, acc);
  }
  for (int y = 0; y < th; ++y) {
    const uint32_t* src = tile + y * kTile;
    if (target.out32) std::memcpy(target.out32 + (ty + y) * target.w + tx, src, sizeof(uint32_t) * tw);
    if (target.out565) {
      uint16_t* o = target.out565 + (ty + y) * target.w + tx;
      for (int x = 0; x < tw; ++x) o[x] = to565(src[x], tx + x, ty + y);
    }
  }
}

void Renderer::run_tiles(void* p) {
  TileJob& job = *static_cast<TileJob*>(p);
  TileQueue& q = *job.queue;
  const Target& target = *q.target;
  for (;;) {
    const int k = q.next.fetch_add(1, std::memory_order_relaxed);
    if (k >= q.count) break;
    const int t = q.todo[k];
    const int tx = (t % q.cols) * kTile, ty = (t / q.cols) * kTile;
    const int tw = target.w - tx < kTile ? target.w - tx : kTile;
    const int th = target.h - ty < kTile ? target.h - ty : kTile;
    draw_tile(*q.list, target, tx, ty, tw, th, job.scratch->tile, job.scratch->acc);
    ++job.done;
  }
}

RenderStats Renderer::render(const DisplayList& list, const Target& target, TileCache* cache, Worker* worker) {
  RenderStats stats;
  const int cols = (target.w + kTile - 1) / kTile;
  const int rows = (target.h + kTile - 1) / kTile;
  if (cache && (cols > TileCache::kCols || rows > TileCache::kRows)) cache = nullptr;
  // Tile lists are bounded by the cache grid; larger targets render serially.
  if (cols * rows > kMaxTiles) worker = nullptr;
  bool dirty[TileCache::kCols * TileCache::kRows]{};
  uint16_t todo[kMaxTiles];
  int todo_count = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int tx = c * kTile, ty = r * kTile;
      const int tw = target.w - tx < kTile ? target.w - tx : kTile;
      const int th = target.h - ty < kTile ? target.h - ty : kTile;
      uint32_t h = 0x9E3779B9u;
      for (uint32_t i = 0; i < list.item_count_; ++i) {
        const Item& it = list.items_[i];
        if (it.x1 <= tx || it.x0 >= tx + tw || it.y1 <= ty || it.y0 >= ty + th) continue;
        h = mix(h, it.hash);
      }
      const int t = r * TileCache::kCols + c;
      if (cache) {
        if (cache->valid && cache->hash[t] == h) {
          ++stats.tiles_skipped;
          continue;
        }
        cache->hash[t] = h;
      }
      if (cols * rows <= kMaxTiles) {
        todo[todo_count++] = static_cast<uint16_t>(r * cols + c);
      } else {
        draw_tile(list, target, tx, ty, tw, th, g_scratch[0].tile, g_scratch[0].acc);
      }
      ++stats.tiles_drawn;
      if (c < TileCache::kCols && r < TileCache::kRows) dirty[t] = true;
    }
  }
  if (todo_count > 0) {
    TileQueue queue{&list, &target, todo, todo_count, cols, {0}};
    TileJob mine{&queue, &g_scratch[0], 0};
    TileJob theirs{&queue, &g_scratch[1], 0};
    const bool helped = worker && todo_count > 1 && worker->start(&Renderer::run_tiles, &theirs);
    run_tiles(&mine);
    if (helped) worker->join();
    stats.tiles_by_worker = theirs.done;
  }
  if (cache) cache->valid = true;

  // Dirty rectangles: horizontal runs per tile row, merged with an identical
  // run directly above.
  for (int r = 0; r < rows && r < TileCache::kRows; ++r) {
    int c = 0;
    while (c < cols) {
      if (!dirty[r * TileCache::kCols + c]) {
        ++c;
        continue;
      }
      int e = c;
      while (e < cols && dirty[r * TileCache::kCols + e]) ++e;
      const int16_t x = static_cast<int16_t>(c * kTile);
      const int16_t w = static_cast<int16_t>((e * kTile < target.w ? e * kTile : target.w) - x);
      const int16_t y = static_cast<int16_t>(r * kTile);
      const int16_t h = static_cast<int16_t>(((r + 1) * kTile < target.h ? (r + 1) * kTile : target.h) - y);
      bool merged = false;
      for (int i = 0; i < stats.rect_count; ++i) {
        int16_t* q = stats.rects[i];
        if (q[0] == x && q[2] == w && q[1] + q[3] == y) {
          q[3] = static_cast<int16_t>(q[3] + h);
          merged = true;
          break;
        }
      }
      if (!merged) {
        if (stats.rect_count == RenderStats::kMaxRects) {
          // Too fragmented: report the whole target so nothing is missed.
          stats.rect_count = 1;
          stats.rects[0][0] = 0;
          stats.rects[0][1] = 0;
          stats.rects[0][2] = static_cast<int16_t>(target.w);
          stats.rects[0][3] = static_cast<int16_t>(target.h);
          return stats;
        }
        int16_t* q = stats.rects[stats.rect_count++];
        q[0] = x;
        q[1] = y;
        q[2] = w;
        q[3] = h;
      }
      c = e;
    }
  }
  return stats;
}

#ifdef ESP_PLATFORM
namespace {
// Raster helper pinned to core 0 (the UI task runs on core 1). Priority 4 keeps
// it below the audio task (6) so sound never starves.
class CoreWorker final : public Worker {
 public:
  bool start(void (*fn)(void*), void* arg) override {
    if (!task_) {
      done_ = xSemaphoreCreateBinary();
      if (!done_ || xTaskCreatePinnedToCore(&CoreWorker::loop, "c2d", 3072, this, 4, &task_, 0) != pdPASS) {
        task_ = nullptr;
        return false;
      }
    }
    fn_ = fn;
    arg_ = arg;
    xTaskNotifyGive(task_);
    return true;
  }
  void join() override { xSemaphoreTake(done_, portMAX_DELAY); }

 private:
  static void loop(void* self) {
    CoreWorker& w = *static_cast<CoreWorker*>(self);
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      w.fn_(w.arg_);
      xSemaphoreGive(w.done_);
    }
  }
  TaskHandle_t task_ = nullptr;
  SemaphoreHandle_t done_ = nullptr;
  void (*fn_)(void*) = nullptr;
  void* arg_ = nullptr;
};
}  // namespace

Worker* second_core_worker() {
  static CoreWorker worker;
  return &worker;
}
#else
Worker* second_core_worker() { return nullptr; }
#endif

}  // namespace little_arcade::ui::c2d
