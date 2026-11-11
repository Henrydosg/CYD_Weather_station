#include "scene_display.h"

#include <stdlib.h>
#include <string.h>

namespace tf {

namespace {
constexpr float kPi = 3.14159265f;
constexpr size_t kStripPixels = static_cast<size_t>(kSceneW) * kRegionStripRows;
// A dirty band is at most the grid width and kBandRows rows: it must fit the strip.
static_assert(ScenePainter::cellsScreenPx(kGridW) * ScenePainter::cellsScreenPx(kBandRows) <=
                  kSceneW * kRegionStripRows,
              "dirty band larger than the strip buffer");
}  // namespace

bool SceneDisplay::begin() {
  if (shown_ == nullptr) {
    shown_ = static_cast<uint8_t*>(malloc(ScenePainter::kKeyBytes));
  }
  if (strip_ == nullptr) {
    strip_ = static_cast<uint16_t*>(malloc(kStripPixels * sizeof(uint16_t)));
  }
  if (shown_ == nullptr || strip_ == nullptr) {
    return false;
  }
  invalidate();
  return true;
}

void SceneDisplay::invalidate() {
  memset(shown_, ScenePainter::kKeyInvalid, ScenePainter::kKeyBytes);
}

void SceneDisplay::pushRegion(const SandSim& sim, float angle, float scale) {
  for (int row = 0; row < kSceneH; row += kRegionStripRows) {
    const int rows = kSceneH - row < kRegionStripRows ? kSceneH - row : kRegionStripRows;
    painter_.paintRegion(sim, shown_, angle, scale, row, rows, strip_);
    tft_.pushImage(kSceneX, kSceneTop + row, kSceneW, rows, strip_);
  }
}

void SceneDisplay::drawAll(const SandSim& sim) {
  painter_.computeKeys(sim, shown_, false);
  pushRegion(sim, 0.0f, 1.0f);
  forgetAir();
}

uint32_t SceneDisplay::drawDirty(const SandSim& sim) {
  uint32_t pixels = 0;
  for (int cy0 = 0; cy0 < kGridH; cy0 += kBandRows) {
    const int cy1 = cy0 + kBandRows - 1 < kGridH ? cy0 + kBandRows - 1 : kGridH - 1;
    int minX = kGridW, maxX = -1;
    for (int cy = cy0; cy <= cy1; ++cy) {
      uint8_t* shownRow = shown_ + cy * kGridW;
      for (int cx = sim.rowLeft(cy); cx <= sim.rowRight(cy); ++cx) {
        const uint8_t key = painter_.cellKey(sim, cx, cy);
        if (key != shownRow[cx]) {
          shownRow[cx] = key;
          if (cx < minX) minX = cx;
          if (cx > maxX) maxX = cx;
        }
      }
    }
    if (maxX < 0) {
      continue;
    }
    const int w = ScenePainter::cellsScreenPx(maxX - minX + 1);
    const int h = ScenePainter::cellsScreenPx(cy1 - cy0 + 1);
    painter_.paintCells(sim, shown_, minX, cy0, maxX, cy1, strip_);
    tft_.pushImage(ScenePainter::cellScreenX(minX), ScenePainter::cellScreenY(cy0), w, h, strip_);
    pixels += static_cast<uint32_t>(w) * h;
  }
  return pixels;
}

void SceneDisplay::beginRotation(const SandSim& sim) {
  painter_.computeKeys(sim, shown_, true);  // grains at rest while turning
}

void SceneDisplay::drawRotation(const SandSim& sim, float fraction) {
  const float angle = fraction * kPi;
  // The old catchers dissolve while the glass turns.
  ScenePainter::fadeCatchers(shown_, static_cast<int>((1.0f - fraction) * ScenePainter::kCatcherFades) - 1);
  pushRegion(sim, angle, ScenePainter::fitScale(angle));
}

void SceneDisplay::endRotation(const SandSim& sim) {
  drawAll(sim);
  forgetAir();
}

void SceneDisplay::forgetAir() {
  for (MotePixels& m : motes_) {
    m = MotePixels{};
  }
}

// A dot only where all its pixels are plain background.
bool SceneDisplay::backgroundDot(int x, int y) const {
  for (int oy = 0; oy < kAirDotPx; ++oy) {
    for (int ox = 0; ox < kAirDotPx; ++ox) {
      const int sx = kSceneX + x + ox, sy = kSceneTop + y + oy;
      if (x + ox >= kSceneW || y + oy >= kSceneH || !painter_.isBackgroundPixel(x + ox, y + oy) ||
          (airMask_ != nullptr && airMask_(static_cast<int16_t>(sx), static_cast<int16_t>(sy)))) {
        return false;
      }
    }
  }
  return true;
}

void SceneDisplay::drawDot(int x, int y, uint16_t colour) {
  tft_.fillRect(kSceneX + x, kSceneTop + y, kAirDotPx, kAirDotPx, colour);
}

void SceneDisplay::drawAir(const AirField& air) {
  const uint16_t bg = painter_.background();
  for (int i = 0; i < air.count(); ++i) {
    MotePixels& p = motes_[i];
    // Motes snap to a 2-pixel grid, so head and tail never overlap.
    const int16_t x = static_cast<int16_t>(static_cast<int>(air.mote(i).x) / kAirDotPx * kAirDotPx);
    const int16_t y = static_cast<int16_t>(static_cast<int>(air.mote(i).y) / kAirDotPx * kAirDotPx);
    if (x == p.hx && y == p.hy) {
      drawDot(x, y, air.colour(i, false));  // same place, fading in / out
      continue;
    }
    // Old tail back to background, old head becomes the tail.
    if (p.tx >= 0 && !(p.tx == x && p.ty == y)) {
      drawDot(p.tx, p.ty, bg);
    }
    p.tx = p.hx;
    p.ty = p.hy;
    if (p.tx >= 0) {
      drawDot(p.tx, p.ty, air.colour(i, true));
    }
    if (backgroundDot(x, y)) {
      drawDot(x, y, air.colour(i, false));
      p.hx = x;
      p.hy = y;
    } else {
      p.hx = p.hy = -1;  // behind the glass or the frame
    }
  }
}

}  // namespace tf
