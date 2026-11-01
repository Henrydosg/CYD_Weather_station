// Display backlight on LEDC PWM: a fixed level, or following the light sensor
// (LDR) so the screen dims in a dark room. Changes fade in over a few samples.
#pragma once

#include <stdint.h>

#include "../settings/settings.h"

class Backlight {
 public:
  // After tft.init() (which switches the pin on as a plain output).
  void begin();
  void update(uint32_t now, Brightness mode);

 private:
  uint8_t targetPct(Brightness mode) const;
  void write(uint8_t pct);

  uint32_t lastSampleAt_ = 0;
  uint32_t lastLogAt_ = 0;
  float ambient_ = -1;  // smoothed raw LDR reading, -1 = no sample yet
  uint8_t pct_ = 100;
};
