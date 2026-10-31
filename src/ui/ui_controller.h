// Owns the screens, routes touch events to the active one and reports what
// the rest of the system must do (refresh, persist settings).
#pragma once

#include <TFT_eSPI.h>

#include "../settings/settings.h"
#include "screen_calibration.h"
#include "screen_connect.h"
#include "screen_keyboard.h"
#include "screen_main.h"
#include "screen_settings.h"
#include "screen_wifi.h"
#include "touch_input.h"

enum class UiAction : uint8_t {
  None,
  RefreshWeather,
  LanguageChanged,   // see UiCommand::language
  CalibrationSaved,  // see UiCommand::calibration
  ScanWifi,
  ConnectWifi,       // see UiCommand::ssid / password
  ForgetWifi,        // see UiCommand::ssid
};

struct UiCommand {
  UiAction action = UiAction::None;
  Language language = Language::Vietnamese;
  TouchCalibration calibration{};
  char ssid[config::kMaxSsidLen + 1] = "";
  char password[config::kMaxPasswordLen + 1] = "";
};

class UiController {
 public:
  explicit UiController(TFT_eSPI& tft);

  // Sets up the display. Starts with calibration if it was never done.
  void begin(const Settings& settings);
  // Call every loop iteration.
  UiCommand update(const UiState& state, const TouchEvent& touch, uint32_t now);

 private:
  enum class Screen : uint8_t { Main, Settings, Calibration, Wifi, Keyboard, Connect };

  void showMain();
  void showSettings(const UiState& state);
  void showCalibration(Screen returnTo);
  void showWifi(const UiState& state, bool resetPage);
  UiCommand changeLanguage(Language lang, const UiState& state);
  UiCommand pickNetwork(const WifiNetworkInfo& net, const UiState& state);
  UiCommand connect(const char* ssid, const char* password, bool secured, const UiState& state);
  UiCommand updateWifi(const UiState& state, const TouchEvent& touch);
  UiCommand updateKeyboard(const UiState& state, const TouchEvent& touch, uint32_t now);
  UiCommand updateConnect(const UiState& state, const TouchEvent& touch, uint32_t now);

  TFT_eSPI& tft_;
  ScreenMain main_;
  ScreenSettings settings_;
  ScreenCalibration calibration_;
  ScreenWifi wifi_;
  ScreenKeyboard keyboard_;
  ScreenConnect connect_;
  Screen screen_ = Screen::Main;
  Screen afterCalibration_ = Screen::Main;
};
