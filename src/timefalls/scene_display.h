// Pushes the hourglass scene to the TFT without flashing: the screen is never
// cleared. Upright frames send only the cells whose visual key changed (dirty
// spans per band of rows); rotation frames repaint the whole scene region in
// strips (each pixel written once per frame). Buffers are allocated once.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

#include "air_field.h"
#include "scene_painter.h"
#include "sand_sim.h"

namespace tf {

class SceneDisplay {
 public:
  SceneDisplay(TFT_eSPI& tft, ScenePainter& painter) : tft_(tft), painter_(painter) {}

  bool begin();  // false if a buffer could not be allocated
  // Whole region, upright (after boot, after a flip).
  void drawAll(const SandSim& sim);
  // Changed cells only. Returns the number of pixels sent.
  uint32_t drawDirty(const SandSim& sim);
  // Every cell is redrawn on the next drawDirty() (palette change).
  void invalidate();

  void beginRotation(const SandSim& sim);  // freezes the grain image
  void drawRotation(const SandSim& sim, float fraction);  // 0..1 of 180 degrees
  void endRotation(const SandSim& sim);    // after sim.flip(): upright frame

  // Air motes on the background around the glass: each mote is a head pixel
  // and a dimmer tail pixel; only pixels that change are written.
  void drawAir(const AirField& air);
  void forgetAir();  // the region was repainted (rotation, full redraw)
  // Screen pixels where no mote may be drawn (text drawn by someone else).
  void setAirMask(bool (*covered)(int16_t x, int16_t y)) { airMask_ = covered; }

 private:
  void pushRegion(const SandSim& sim, float angle, float scale);
  bool backgroundDot(int x, int y) const;
  void drawDot(int x, int y, uint16_t colour);

  TFT_eSPI& tft_;
  ScenePainter& painter_;
  uint8_t* shown_ = nullptr;  // visual key currently on screen, per cell
  uint16_t* strip_ = nullptr;  // kSceneW x kRegionStripRows pixels
  struct MotePixels {
    int16_t hx = -1, hy = -1, tx = -1, ty = -1;
  };
  MotePixels motes_[kAirMotes];
  bool (*airMask_)(int16_t, int16_t) = nullptr;
};

}  // namespace tf
