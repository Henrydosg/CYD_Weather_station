// XPT2046 touch input over bit-banged SPI (the TFT uses HSPI, the SD card
// VSPI). Produces one debounced tap event per touch, with screen coordinates.
#pragma once

#include <stdint.h>

#include "../settings/settings.h"

struct TouchEvent {
  bool tapped = false;
  int16_t x = 0, y = 0;        // screen pixels (calibrated)
  int16_t rawX = 0, rawY = 0;  // controller units (for calibration)
};

class TouchInput {
 public:
  void begin();
  void setCalibration(const TouchCalibration& cal) { cal_ = cal; }
  // Call every loop iteration. Never blocks.
  TouchEvent poll(uint32_t now);

 private:
  // One pressure + position measurement; false when not pressed.
  bool read(int16_t& rawX, int16_t& rawY);
  uint8_t transfer(uint8_t out);
  uint16_t transfer16(uint16_t out);

  TouchCalibration cal_{};
  bool wasTouched_ = false;
  uint32_t lastTap_ = 0;
};
