// Settings screen: language selector, touch calibration entry, WiFi status
// (tap it to pick a network).
#pragma once

#include <TFT_eSPI.h>

#include "screen_main.h"

enum class SettingsTarget : uint8_t {
  None,
  Back,
  Vietnamese,
  English,
  Calibrate,
  Wifi,
};

class ScreenSettings {
 public:
  explicit ScreenSettings(TFT_eSPI& tft) : tft_(tft) {}

  // Full redraw.
  void show(const UiState& state);
  // Redraws the WiFi row when the connection changes.
  void update(const UiState& state);

  SettingsTarget hitTest(int16_t x, int16_t y) const;

 private:
  void drawLanguageRow();
  void drawWifiRow(const UiState& state);

  TFT_eSPI& tft_;
  WifiState lastWifi_ = WifiState::Idle;
  char lastSsid_[config::kMaxSsidLen + 1] = "";
};
