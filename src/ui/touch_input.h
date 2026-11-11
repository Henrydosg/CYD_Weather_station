// XPT2046 touch input: bit-banged SPI on its own pins (2.8": the TFT uses
// HSPI, the SD card VSPI) or the TFT's SPI bus (3.5", board::kTouchSharesTftSpi). Produces one debounced tap event per touch, with screen coordinates.
// startTask() polls the panel on its own task, so a tap made while loop() is
// busy (photo decode, a long redraw) waits in a queue instead of being lost.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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
  void setCalibration(const TouchCalibration& cal);
  // Polls every config::kTouchPollMs on a task of its own; false if it could
  // not be created (then call poll() from loop() instead).
  bool startTask();
  // Next queued tap (tapped = false when none). Without the task: poll(now).
  TouchEvent take(uint32_t now);
  // One debounced reading. Never blocks.
  TouchEvent poll(uint32_t now);

 private:
  // One pressure + position measurement; false when not pressed.
  bool read(int16_t& rawX, int16_t& rawY);
  uint8_t transfer(uint8_t out);
  uint16_t transfer16(uint16_t out);

  static void taskEntry(void* self);

  TouchCalibration cal_{};
  portMUX_TYPE calLock_ = portMUX_INITIALIZER_UNLOCKED;
  QueueHandle_t queue_ = nullptr;
  bool wasTouched_ = false;
  uint32_t lastTap_ = 0;
};
