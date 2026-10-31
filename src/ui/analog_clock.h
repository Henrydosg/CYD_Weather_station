// Analog clock with a digital readout underneath (inside a card). Both are
// rendered into off-screen sprites and pushed in one go, so the
// once-per-second redraw does not flicker.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

class AnalogClock {
 public:
  // Size of the digital readout below the face.
  static constexpr int16_t kDigitalW = 120;
  static constexpr int16_t kDigitalH = 28;

  AnalogClock(TFT_eSPI& tft, int16_t faceX, int16_t faceY, int16_t textX, int16_t textY);

  void begin();
  // Forces a redraw on the next update() (after the area was painted over).
  void invalidate() { dirty_ = true; }
  // Redraws only when the second changes. `t` is nullptr while unsynced.
  void update(const struct tm* t);

 private:
  void drawFace(const struct tm* t);
  void drawDigital(const struct tm* t);
  void drawHand(float angleDeg, float length, float tail, float baseR, float tipR, uint16_t color);

  TFT_eSPI& tft_;
  TFT_eSprite face_;
  TFT_eSprite text_;
  int16_t faceX_, faceY_, textX_, textY_;
  bool ready_ = false;
  bool dirty_ = true;
  bool lastValid_ = false;
  int lastSecond_ = -1;
};
