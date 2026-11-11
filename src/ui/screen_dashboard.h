// Main screen: one city at a time, rotating through the saved cities.
// Header (name, region, local time, date + lunar date, signal, position dots),
// current weather on the left with one note line (rain nowcast first, then
// Tet / full moon / new moon / Tet countdown), "label: value" details on the
// right, 7-day strip below.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

#include "../calendar/lunar_calendar.h"
#include "ui_state.h"

enum class DashTarget : uint8_t { Settings, Clock, Graph };

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
  // Shows / hides the ":" of the clock (blinks once a second).
  void drawColon(bool on);
  void drawCurrent(const CityWeather& w);
  // The note line under "feels like"; `force` redraws even when unchanged.
  void drawNote(const UiState& s, bool force);
  // Lunar date and days to Tet of a local date (cached: doubles are slow).
  void updateLunar(const struct tm& t);
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
  bool colonOn_ = true;
  uint8_t lastBars_ = 0;
  WifiState lastWifi_ = WifiState::Idle;
  char lastNote_[48] = "";
  int32_t lunarKey_ = -1;  // yyyymmdd of lunar_ / tetDays_
  LunarDate lunar_{};
  int tetDays_ = -1;
};
