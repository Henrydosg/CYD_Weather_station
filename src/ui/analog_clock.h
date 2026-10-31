// Analog clock with a digital readout underneath (inside a glass card). Both are rendered into
// off-screen sprites (including the sky gradient behind them) and pushed in
// one go, so the once-per-second redraw does not flicker.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

#include "theme.h"

class AnalogClock {
 public:
  // Size of the digital readout below the face.
  static constexpr int16_t kDigitalW = 112;
  static constexpr int16_t kDigitalH = 28;

  AnalogClock(TFT_eSPI& tft, int16_t faceX, int16_t faceY, int16_t textX, int16_t textY);

  void begin();
  // Sets the sky palette and forces a redraw on the next update().
  void setPalette(const theme::Palette& palette);
  // Redraws only when the second changes. `t` is nullptr while unsynced.
  void update(const struct tm* t);

 private:
  // Paints glass (glassAlpha 0 = plain sky) matching screen rows from screenY.
  void paintRows(TFT_eSprite& sprite, int16_t screenY, uint8_t glassAlpha);
  void fillGlassDisc(float radius, uint16_t under);
  void drawFace(const struct tm* t);
  void drawDigital(const struct tm* t);
  void drawHand(float angleDeg, float length, float tail, float baseR, float tipR, uint16_t color);

  TFT_eSPI& tft_;
  TFT_eSprite face_;
  TFT_eSprite text_;
  int16_t faceX_, faceY_, textX_, textY_;
  const theme::Palette* palette_ = &theme::defaultPalette();
  bool ready_ = false;
  bool dirty_ = true;
  bool lastValid_ = false;
  int lastSecond_ = -1;
};
