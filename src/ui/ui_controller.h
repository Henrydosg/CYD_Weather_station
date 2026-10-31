// Owns the screens, routes touch events to the active one and reports what
// the rest of the system must do (persist settings, WiFi, city search).
#pragma once

#include <TFT_eSPI.h>

#include "../settings/settings.h"
#include "screen_calibration.h"
#include "screen_cities.h"
#include "screen_connect.h"
#include "screen_dashboard.h"
#include "screen_graph.h"
#include "screen_keyboard.h"
#include "screen_photos.h"
#include "screen_search.h"
#include "screen_settings.h"
#include "screen_wifi.h"
#include "touch_input.h"
#include "ui_state.h"

class UiController {
 public:
  explicit UiController(TFT_eSPI& tft);

  // Sets up the display. Starts with calibration if it was never done.
  void begin(const Settings& settings);
  // Call every loop iteration.
  UiCommand update(const UiState& state, const TouchEvent& touch, uint32_t now);

 private:
  enum class Screen : uint8_t {
    Dashboard, Graph, Settings, Calibration, Wifi, Keyboard, Connect, Cities, Search, Photos
  };

  void showDashboard();
  void showSettings(const UiState& state);
  void showCalibration(Screen returnTo);
  void showWifi(const UiState& state, bool resetPage);
  UiCommand pickNetwork(const WifiNetworkInfo& net, const UiState& state);
  UiCommand connect(const char* ssid, const char* password, bool secured, const UiState& state);

  UiCommand updateDashboard(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updateSettings(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updatePhotos(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updateWifi(const UiState& state, const TouchEvent& touch);
  UiCommand updateKeyboard(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updateConnect(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updateCities(const UiState& state, const TouchEvent& touch);
  UiCommand updateSearch(const UiState& state, const TouchEvent& touch);

  TFT_eSPI& tft_;
  ScreenDashboard dashboard_;
  ScreenGraph graph_;
  ScreenSettings settings_;
  ScreenCalibration calibration_;
  ScreenWifi wifi_;
  ScreenKeyboard keyboard_;
  ScreenConnect connect_;
  ScreenCities cities_;
  ScreenSearch search_;
  ScreenPhotos photos_;
  Screen screen_ = Screen::Dashboard;
  Screen afterCalibration_ = Screen::Dashboard;
  uint32_t lastTapAt_ = 0;
  bool dashboardPending_ = true;  // full redraw on the next update (needs UiState)
};
