// Main screen: one city at a time, rotating through the saved cities.
// Header (name, region, local time, date, signal, position dots), current
// weather on the left, "label: value" details on the right, 7-day strip below.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class DashTarget : uint8_t { Settings, Graph };

class ScreenDashboard {
 public:
  explicit ScreenDashboard(TFT_eSPI& tft) : tft_(tft) {}

  // Full redraw of the current city; restarts its display time.
  void show(const UiState& state, uint32_t now);
  // Rotation and partial redraws; cheap to call every loop iteration.
  void update(const UiState& state, uint32_t now);
  DashTarget hitTest(int16_t x, int16_t y) const;
  // Stops the rotation for config::kRotatePauseMs (after a tap).
  void pause(uint32_t now);
  uint8_t city() const { return city_; }

 private:
  // Redraws the current city over the previous one; restarts its display time.
  void redraw(const UiState& s, uint32_t now);
  void drawHeader(const UiState& s);
  void drawCurrent(const CityWeather& w);
  void drawDetails(const CityWeather& w);
  void drawDays(const CityWeather& w);
  void drawWaiting(const UiState& s);

  TFT_eSPI& tft_;
  uint8_t city_ = 0;
  uint32_t shownAt_ = 0;
  bool paused_ = false;
  uint32_t pausedAt_ = 0;
  // Inputs of the last redraw
  uint32_t lastData_ = 0;
  uint32_t lastCities_ = 0;
  int lastMinute_ = -1;
  uint8_t lastBars_ = 0;
  WifiState lastWifi_ = WifiState::Idle;
};
