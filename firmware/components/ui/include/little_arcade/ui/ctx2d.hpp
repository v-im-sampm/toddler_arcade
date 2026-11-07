#pragma once
#include <cstddef>
#include <cstdint>

// Small software implementation of the HTML canvas 2D drawing model, so a
// game can be ported line by line from a canvas reference sketch (for example
// v2/reference/quality_bar.html) and look the same on the desktop simulator
// and the device. LVGL-free and host-tested.
//
// Model: Context records fills and strokes (strokes are expanded to polygons
// at record time) into a DisplayList. render() rasterizes that list in 32x32
// tiles with analytic anti-aliasing (nonzero winding, signed-area
// accumulation), 8-bit source-over / 'lighter' compositing on top of an
// optional cached background, then writes XRGB8888 and/or ordered-dithered
// RGB565. A TileCache skips tiles whose items are unchanged since the last
// frame, which keeps per-frame cost proportional to what actually moved.
//
// All storage is allocated once (DisplayList::allocate); nothing allocates
// per frame.
namespace little_arcade::ui::c2d {

class Context;
class Renderer;

struct Mat {
  // x' = a*x + c*y + e ; y' = b*x + d*y + f  (canvas setTransform order)
  float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
};

struct Color {
  uint8_t r = 0, g = 0, b = 0;
  float a = 1.0f;
};

constexpr Color rgb(uint32_t hex, float alpha = 1.0f) {
  return Color{static_cast<uint8_t>(hex >> 16), static_cast<uint8_t>(hex >> 8), static_cast<uint8_t>(hex), alpha};
}
// CSS hsla(): h in degrees, s/l in percent (0..100).
Color hsla(double h, double s, double l, float a);

enum class Cap : uint8_t { BUTT, ROUND };
enum class Join : uint8_t { MITER, ROUND, BEVEL };
enum class Op : uint8_t { SOURCE_OVER, LIGHTER };

inline constexpr int kTile = 32;

struct Paint {
  int16_t grad = -1;  // >= 0: gradient index
  Color color{};
};

struct Gradient {
  bool radial = false;
  float x0 = 0, y0 = 0, r0 = 0, x1 = 0, y1 = 0, r1 = 0;
  uint8_t stop_count = 0;
  float offsets[4]{};
  Color stops[4]{};
  bool lut_ready = false;
  uint32_t lut[256]{};  // premultiplied-free ARGB, alpha in top byte
  uint32_t hash = 0;
};

struct Item {
  uint32_t first_contour = 0;
  uint32_t contour_count = 0;
  int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // device bbox, exclusive max
  Paint paint{};
  Mat inv{};  // device -> gradient space (gradients only)
  float alpha = 1.0f;
  Op op = Op::SOURCE_OVER;
  uint32_t hash = 0;
};

struct Pt {
  float x, y;
};

struct Contour {
  uint32_t first = 0;
  uint32_t count = 0;
};

// Recorded frame. Capacities are fixed at allocate().
class DisplayList {
 public:
  struct Limits {
    uint32_t points = 16384;
    uint32_t contours = 3072;
    uint32_t items = 640;
    uint32_t gradients = 16;
  };
  bool allocate(const Limits& limits);
  bool allocate() { return allocate(Limits{}); }
  bool ready() const { return pts_ != nullptr; }
  void clear();

  uint32_t item_count() const { return item_count_; }
  const Item& item(uint32_t i) const { return items_[i]; }
  uint32_t point_count() const { return pt_count_; }
  uint32_t contour_total() const { return contour_count_; }
  // Records dropped because a pool was full (should stay 0; checked in tests).
  uint32_t overflows() const { return overflows_; }
  const Limits& limits() const { return limits_; }
  std::size_t bytes() const;

 private:
  friend class Context;
  friend class Renderer;

  static constexpr uint32_t kPathMax = 2048;

  Limits limits_{};
  Pt* path_ = nullptr;     // Context's current path (device space)
  Pt* scratch_ = nullptr;  // stroker workspace (user space)
  Pt* pts_ = nullptr;
  Contour* contours_ = nullptr;
  Item* items_ = nullptr;
  Gradient* grads_ = nullptr;
  uint32_t pt_count_ = 0;
  uint32_t contour_count_ = 0;
  uint32_t item_count_ = 0;
  uint32_t grad_count_ = 0;
  uint32_t overflows_ = 0;
};

// Canvas-like recorder. Method names mirror CanvasRenderingContext2D.
class Context {
 public:
  explicit Context(DisplayList& list) : list_(list) {}

  // Clears the display list and resets all state (identity transform,
  // black paint, lineWidth 1, butt caps, miter joins, alpha 1).
  void reset();

  void save();
  void restore();
  void translate(double x, double y);
  void rotate(double radians);
  void scale(double sx, double sy);
  void set_transform(const Mat& m) { st_.m = m; }
  const Mat& transform() const { return st_.m; }

  void set_fill(Color c) { st_.fill = Paint{-1, c}; }
  void set_fill_gradient(int g) { st_.fill = Paint{static_cast<int16_t>(g), Color{}}; }
  void set_stroke(Color c) { st_.stroke = Paint{-1, c}; }
  void set_line_width(double w) { st_.line_width = static_cast<float>(w); }
  void set_line_cap(Cap c) { st_.cap = c; }
  void set_line_join(Join j) { st_.join = j; }
  void set_global_alpha(double a);
  double global_alpha() const { return st_.alpha; }
  void set_composite(Op op) { st_.op = op; }

  // Returns a gradient handle (-1 when the pool is full).
  int create_linear_gradient(double x0, double y0, double x1, double y1);
  int create_radial_gradient(double x0, double y0, double r0, double x1, double y1, double r1);
  void add_color_stop(int g, double offset, Color c);

  void begin_path();
  void move_to(double x, double y);
  void line_to(double x, double y);
  void close_path();
  void quadratic_curve_to(double cx, double cy, double x, double y);
  void bezier_curve_to(double c1x, double c1y, double c2x, double c2y, double x, double y);
  void arc(double x, double y, double r, double a0, double a1, bool ccw = false);
  void ellipse(double x, double y, double rx, double ry, double rot, double a0, double a1, bool ccw = false);
  void arc_to(double x1, double y1, double x2, double y2, double r);

  void fill();
  void stroke();
  void fill_rect(double x, double y, double w, double h);

  uint32_t path_points() const { return path_count_; }

  // Max distance (device px) between a curve and its flattened polygon.
  // Kept across reset(); the default suits full-size art, a game may trade a
  // little smoothness for fewer points (and faster recording/raster).
  static constexpr float kDefaultFlatTol = 0.05f;
  void set_flatten_tolerance(float px) { flat_tol_ = px > 0.01f ? px : 0.01f; }
  float flatten_tolerance() const { return flat_tol_; }

 private:
  static constexpr int kStack = 12;
  static constexpr uint32_t kSubMax = 96;

  struct State {
    Mat m{};
    Paint fill{-1, Color{0, 0, 0, 1}};
    Paint stroke{-1, Color{0, 0, 0, 1}};
    float line_width = 1;
    Cap cap = Cap::BUTT;
    Join join = Join::MITER;
    float alpha = 1;
    Op op = Op::SOURCE_OVER;
  };
  struct Sub {
    uint32_t first;
    uint32_t count;
    bool closed;
  };

  Pt map(double x, double y) const;
  void add_device(Pt p);
  float device_scale() const;
  void arc_points(float cx, float cy, float rx, float ry, float rot, float a0, float sweep);

  // Display-list emission.
  bool begin_item();
  void begin_contour();
  void emit(Pt p);
  void end_contour();
  void end_item(const Paint& paint);
  void emit_polygon_user(const Pt* user, uint32_t n, const Mat& m);
  void emit_circle_user(Pt c, float r, const Mat& m, float dev_scale);
  void stroke_sub(const Pt* user, uint32_t n, bool closed, const Mat& m, float dev_scale);

  DisplayList& list_;
  State st_{};
  State stack_[kStack]{};
  int depth_ = 0;
  uint32_t path_count_ = 0;
  Sub subs_[kSubMax]{};
  uint32_t sub_count_ = 0;
  bool have_current_ = false;
  float flat_tol_ = kDefaultFlatTol;
  // Item under construction.
  uint32_t item_first_contour_ = 0;
  uint32_t item_first_pt_ = 0;
  uint32_t contour_first_pt_ = 0;
  float bx0_ = 0, by0_ = 0, bx1_ = 0, by1_ = 0;
  bool item_open_ = false;
  bool item_failed_ = false;
};

// Per-tile hashes from the previous frame; lets render() skip unchanged tiles.
struct TileCache {
  static constexpr int kCols = (320 + kTile - 1) / kTile;
  static constexpr int kRows = (240 + kTile - 1) / kTile;
  uint32_t hash[kCols * kRows]{};
  bool valid = false;
  void invalidate() { valid = false; }
};

struct Target {
  int w = 320;
  int h = 240;
  const uint32_t* background = nullptr;  // XRGB8888 w*h, or null (black)
  uint32_t* out32 = nullptr;             // optional XRGB8888 w*h
  uint16_t* out565 = nullptr;            // optional RGB565 w*h (ordered dither)
};

struct RenderStats {
  int tiles_drawn = 0;
  int tiles_skipped = 0;
  int tiles_by_worker = 0;
  // Dirty region as tile-aligned rectangles (merged per row, then vertically).
  static constexpr int kMaxRects = 40;
  int rect_count = 0;
  int16_t rects[kMaxRects][4]{};  // x, y, w, h in pixels
};

// Runs one job on another core while the caller works (Renderer::render).
class Worker {
 public:
  virtual ~Worker() = default;
  // Starts fn(arg) asynchronously; false means the caller must do the work.
  virtual bool start(void (*fn)(void*), void* arg) = 0;
  virtual void join() = 0;
};

// The ESP32-S3's other core (core 0, below the audio task's priority), created
// on first use. Null on the host.
Worker* second_core_worker();

class Renderer {
 public:
  // Renders `list` into `target`. With a cache, tiles whose content hash is
  // unchanged are left untouched. w/h must be <= 320x240 when a cache is used.
  // With a worker, dirty tiles are shared between the caller and the worker;
  // the output is identical to a single-threaded render.
  static RenderStats render(const DisplayList& list, const Target& target, TileCache* cache,
                            Worker* worker = nullptr);

 private:
  static void raster_item(const DisplayList& list, const Item& it, int tx, int ty, int tw, int th, uint32_t* tile,
                          float* acc);
  static void draw_tile(const DisplayList& list, const Target& target, int tx, int ty, int tw, int th, uint32_t* tile,
                        float* acc);
  static void run_tiles(void* job);
};

// Converts one XRGB pixel to RGB565 using the same 4x4 ordered dither as
// render() (exposed for tests and the background cache).
uint16_t to565(uint32_t xrgb, int x, int y);

}  // namespace little_arcade::ui::c2d
