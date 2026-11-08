// Device information (Settings -> Firmware -> Info): version and whether it is
// confirmed after an update, uptime, boots / crashes, last reset reason, WiFi,
// IP, MAC, free RAM and the age of the weather data. One label / value row each.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

class ScreenInfo {
 public:
  explicit ScreenInfo(TFT_eSPI& tft) : tft_(tft) {}

  void show(const UiState& state);
  // Once a second: redraws the values that changed.
  void update(const UiState& state);
  bool hitBack(int16_t x, int16_t y) const;

  static constexpr int kRows = 9;

 private:
  void drawValues(const UiState& state, bool force);

  TFT_eSPI& tft_;
  uint32_t lastUptime_ = 0;
  char last_[kRows][48] = {};
  uint16_t lastColor_[kRows] = {};
};
