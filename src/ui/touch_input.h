// XPT2046 touch input on its own SPI bus (the TFT uses HSPI).
// Produces one debounced tap event per touch, with screen coordinates.
#pragma once

#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "../settings/settings.h"

struct TouchEvent {
  bool tapped = false;
  int16_t x = 0, y = 0;        // screen pixels (calibrated)
  int16_t rawX = 0, rawY = 0;  // controller units (for calibration)
};

class TouchInput {
 public:
  TouchInput();

  void begin();
  void setCalibration(const TouchCalibration& cal) { cal_ = cal; }
  // Call every loop iteration. Never blocks.
  TouchEvent poll(uint32_t now);

 private:
  SPIClass spi_;
  XPT2046_Touchscreen ts_;
  TouchCalibration cal_{};
  bool wasTouched_ = false;
  uint32_t lastTap_ = 0;
};
