// Two-point touch calibration: the user taps a "+" near the top-left and then
// near the bottom-right corner; the raw readings give the linear mapping.
#pragma once

#include <TFT_eSPI.h>

#include "../settings/settings.h"
#include "theme.h"
#include "touch_input.h"

class ScreenCalibration {
 public:
  explicit ScreenCalibration(TFT_eSPI& tft) : tft_(tft) {}

  void start(const theme::Palette& palette);
  void handleTap(const TouchEvent& ev, uint32_t now);
  // True once the result has been shown long enough to leave the screen.
  bool finished(uint32_t now) const;
  const TouchCalibration& result() const { return result_; }

 private:
  enum class Step : uint8_t { First, Second, Done };

  void drawStep(const char* message);
  void drawTarget(int16_t x, int16_t y, uint16_t color);

  TFT_eSPI& tft_;
  const theme::Palette* palette_ = &theme::defaultPalette();
  Step step_ = Step::First;
  int16_t raw1X_ = 0, raw1Y_ = 0;
  TouchCalibration result_{};
  uint32_t doneAt_ = 0;
};
