// Paints the hourglass scene into RGB565 buffers. Portable (host previews).
//
// The static assembly (glass, outline, champagne frame) is rendered once at
// boot with anti-aliasing into an 8-bit indexed layer (kAsmW x kAsmH bytes,
// checked malloc) and kept point-symmetric, so the image at 180 degrees is
// pixel-identical to the upright one after the grains are remapped.
//
// Grains are drawn from a per-cell "visual key" (empty / layer + shade +
// rest|moving|spark / trail). Brightness effects only pick another tone of
// the grain's own colour; the grain's layer never changes.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sand_sim.h"
#include "timefalls_config.h"

namespace tf {

class ScenePainter {
 public:
  static constexpr int kStates = 3;  // rest, moving, spark
  static constexpr int kKeyTrailBase = 1 + kLayers * kShades * kStates;
  static constexpr int kKeyCatcherBase = kKeyTrailBase + kLayers;  // + fade 0..3
  static constexpr int kCatcherFades = 4;
  static constexpr int kKeyGhostBase = kKeyCatcherBase + kCatcherFades;  // hour digits
  static constexpr int kKeyMagnetBase = kKeyGhostBase + kCatcherFades;   // glow: core, halo
  static constexpr int kKeyCount = kKeyMagnetBase + 2;
  static constexpr uint8_t kKeyInvalid = 0xFF;
  static constexpr size_t kKeyBytes = static_cast<size_t>(kGridW) * kGridH;

  // Allocates and renders the static layer. False if out of memory.
  bool begin(const SandSim& sim);
  void setPalette(uint8_t index);
  uint8_t palette() const { return palette_; }
  static const char* paletteName(uint8_t index);

  // Visual key of one cell; restOnly ignores motion effects (rotation).
  uint8_t cellKey(const SandSim& sim, int cx, int cy, bool restOnly = false) const;
  void computeKeys(const SandSim& sim, uint8_t* keys, bool restOnly) const;
  // Sets every catcher cell in keys to the given fade (0 = faint .. 3), or
  // removes catchers from the image (fade < 0).
  static void fadeCatchers(uint8_t* keys, int fade);

  // Upright: the screen pixels of cells [cx0..cx1] x [cy0..cy1] (row-major,
  // width cellsScreenW(cx1 - cx0 + 1), each cell kCellPx x kView pixels).
  // Screen position: cellScreenX / cellScreenY.
  void paintCells(const SandSim& sim, const uint8_t* keys, int cx0, int cy0, int cx1,
                  int cy1, uint16_t* out) const;
  static int16_t cellScreenX(int cx) {
    return kAsmX + (kInnerLeft + cx * kCellPx) * kViewNum / kViewDen;
  }
  static int16_t cellScreenY(int cy) {
    return kAsmY + (kInnerTop + cy * kCellPx) * kViewNum / kViewDen;
  }
  static constexpr int cellsScreenPx(int cells) { return cells * kCellPx * kViewNum / kViewDen; }

  // Scene region rows [row0, row0 + rows) at full screen width, with the
  // assembly turned by angle (radians, clockwise) and scaled about its centre.
  void paintRegion(const SandSim& sim, const uint8_t* keys, float angle, float scale, int row0,
                   int rows, uint16_t* out) const;
  // Largest scale <= 1 that keeps the turned assembly (shown x kView) inside
  // the region; paintRegion applies kView on top of scale.
  static float fitScale(float angle);

  uint16_t background() const { return background_; }
  // True where the scene region (region pixel coordinates) shows plain
  // background, i.e. outside the glass and the frame: air motes go there.
  bool isBackgroundPixel(int rx, int ry) const;
  uint8_t staticColours() const { return lutCount_; }
  size_t staticBytes() const { return static_cast<size_t>(kAsmW) * kAsmH; }

 private:
  void buildStatic();
  uint8_t lutIndex(uint16_t colour);
  uint16_t samplePixel(const SandSim& sim, const uint8_t* keys, int ix, int iy,
                       int toneFlip) const;

  uint8_t* static_ = nullptr;  // kAsmW x kAsmH palette indices
  uint16_t lut_[256] = {};
  uint8_t lutCount_ = 0;
  uint16_t keyColours_[kKeyCount][4] = {};  // TL, TR, BL, BR of a 2 x 2 grain
  uint16_t background_ = 0;
  uint8_t palette_ = 0;
};

}  // namespace tf
