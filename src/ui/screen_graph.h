// 24 h forecast of one city: temperature line with hour, value and icon at
// every 3rd hour, and the chance of rain as bars. Tap anywhere to go back.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

class ScreenGraph {
 public:
  explicit ScreenGraph(TFT_eSPI& tft) : tft_(tft) {}

  void show(const UiState& state, uint8_t city);
  // Redraws when the data or the city's local minute changes.
  void update(const UiState& state);

 private:
  void drawHeader(const UiState& s);
  void drawPlot(const CityWeather& w);

  TFT_eSPI& tft_;
  uint8_t city_ = 0;
  uint32_t lastData_ = 0;
  int lastMinute_ = -1;
};
