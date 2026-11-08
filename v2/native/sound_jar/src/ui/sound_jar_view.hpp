#pragma once
// LVGL 9 renderer for Sound Jar. Persistent lv_image objects (one per sprite layer, created in the
// reference's drawScene() order) are re-pointed at exported frames and transformed each sync.
// Procedural items (ribbon, loop dots, hint ring, effect halos, rings, particles) are drawn by
// three custom-draw objects placed at the matching z-levels. The view only reads Game state.
#include <cstdint>

#include "game/sound_jar.hpp"
#include "lvgl.h"
#include "ui/asset_pack.hpp"

namespace sound_jar {

class View {
 public:
  // Returns false (and reports the first missing sprite name) if the pack is incomplete.
  bool create(lv_obj_t* screen, const AssetPack& pack, const char** missing = nullptr);
  void sync(const Game& game);

 private:
  struct Layer {
    lv_obj_t* obj = nullptr;
    const Sprite* sprite = nullptr;
    int32_t x = INT32_MIN, y = INT32_MIN, rot = 0;
    uint32_t sx = 256, sy = 256;
    lv_opa_t opa = LV_OPA_COVER;
    uint32_t color = 0xFFFFFFFF;
    bool hidden = false;
    lv_area_t box{0, 0, -1, -1};  // screen bounds last drawn (empty when hidden)
  };
  // A Canvas translate(x, y) -> rotate(rot) -> scale(sx, sy) frame.
  struct Xf {
    double x, y, rot, sx, sy;
    void at(double dx, double dy, double& ox, double& oy) const;
  };
  struct PadLayers { Layer glow, body, eyes, mouth; };
  struct FxLayers { Layer body, icon, ring0, ring1; };
  enum class Pass : uint8_t { Ribbon, Overlay, Fx };
  struct DrawCtx { View* view; Pass pass; };

  static void draw_event(lv_event_t* e);
  void draw(lv_layer_t* layer, Pass pass);
  lv_obj_t* make_draw(lv_obj_t* screen, Pass pass);

  static constexpr int kTile = 20, kTilesX = 320 / kTile, kTilesY = 240 / kTile;
  using Tiles = uint8_t[kTilesY][kTilesX];
  void mark(Tiles& t, double x1, double y1, double x2, double y2) const;
  void dirty_tiles(const Game& game, Tiles& tiles);
  void invalidate(const Tiles& now);
  static constexpr int kMaxRects = 30, kMaxBoxes = 160;
  int tile_rects(const Tiles& now, lv_area_t* rects) const;
  void touched(const lv_area_t& box);  // marks a sprite's old/new bounds for this frame

  const Sprite* need(const char* name);
  Layer make(lv_obj_t* parent, const Sprite* sprite, bool mask, bool additive = false);
  void place(Layer& l, const Sprite* sprite, double x, double y, double rot = 0, double sx = 1, double sy = 1,
                    double opa = 1);
  void place_xf(Layer& l, const Sprite* sprite, const Xf& f, double dx, double dy, double k = 1,
                       double opa = 1);
  void tint(Layer& l, uint32_t rgb);
  void hide(Layer& l);

  const AssetPack* pack_ = nullptr;
  const char* missing_ = nullptr;
  const Game* game_ = nullptr;
  DrawCtx ctx_[3]{};
  lv_obj_t* draw_[3]{};
  Tiles prev_{};
  Tiles sprites_{};  // tiles touched by sprite layer changes during the current sync()
  lv_area_t boxes_[kMaxBoxes]{};  // the same changes as exact screen rectangles
  int box_count_ = 0;
  bool box_overflow_ = false;
  lv_point_precise_t ribbon_[25]{};  // must outlive the deferred line draw tasks

  Layer bg_, specks_[16], glint_l_, drip_main_, drip_drops_[2];
  Layer bin_body_, bin_lid_, rec_dots_[3], rec_arcs_[3], rec_body_;
  Layer jar_back_, mist_, cap_, swirl_, motes_[5], sheen_, cheeks_, eyes_, pupils_, brows_, mouth_, visor_, visor_bar_,
      jar_front_, lid_glow_l_, lid_;
  PadLayers pads_[5];
  FxLayers fx_[4];
  Layer home_, mic_, hand_, start_button_;
  lv_obj_t* veil_ = nullptr;

  const Sprite *speck_[2]{}, *glint_[8]{}, *drip_swell_[8]{}, *drip_fall_ = nullptr, *drip_ring_[12]{},
               *drip_drop_ = nullptr;
  const Sprite *rec_dot_lit_ = nullptr, *rec_dot_dim_ = nullptr, *rec_arc_[19]{};
  const Sprite *mist_body_[5][33]{}, *cap_f_[2][33][8]{}, *sheen_f_[2][33][8]{}, *swirl_f_[17][16]{};
  const Sprite *mote_bubble_[8]{}, *mote_dot_[8]{}, *mote_heart_[8]{}, *mote_star_[8][6]{};
  const Sprite *eyes_open_s_ = nullptr, *eyes_open_l_ = nullptr, *pupils_s_ = nullptr, *pupils_l_ = nullptr,
               *eyes_shut_ = nullptr, *eyes_happy_ = nullptr, *mouth_hmm_ = nullptr, *mouth_o_ = nullptr,
               *mouth_happy_ = nullptr, *mouth_sleep_ = nullptr, *mouth_smile_ = nullptr, *mouth_sing_[16]{};
  const Sprite *lid_s_ = nullptr, *lid_glow_ = nullptr;
  const Sprite *pad_body_[5]{}, *pad_eyes_[5]{}, *pad_smile_[5]{}, *pad_shut_[5]{}, *pad_cheer_[5]{}, *pad_glow_[5]{},
               *pad_sing_[5][8]{};
  const Sprite *fxbody_off_ = nullptr, *fx_body_[4]{}, *fx_icon_[4]{}, *fx0_on_[9]{}, *fx1_dot_ = nullptr,
               *fx1_ring_[2]{}, *fx2_on_[9]{}, *fx3_on_[16]{};
  const Sprite *pcircle_[13]{}, *pstar_[7][6]{}, *prect_[4][12]{}, *pnote_[5]{};
};

}  // namespace sound_jar
