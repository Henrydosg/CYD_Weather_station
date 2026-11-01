// Settings screen: one row each for cities, rotation interval, language,
// touch calibration, WiFi and firmware; "Photos" button in the header.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class SettingsTarget : uint8_t {
  None,
  Back,
  Cities,
  Rotation,  // cycles through config::kRotateChoicesSec
  Language,  // toggles Vietnamese / English
  Calibrate,
  Wifi,
  Firmware,
  Photos,
};

class ScreenSettings {
 public:
  explicit ScreenSettings(TFT_eSPI& tft) : tft_(tft) {}

  // Full redraw.
  void show(const UiState& state);
  // Redraws rows whose values changed (WiFi, rotation, city count).
  void update(const UiState& state);

  SettingsTarget hitTest(int16_t x, int16_t y) const;

 private:
  void drawRow(int row, const char* label, const char* value, uint16_t valueColor);
  void drawCitiesRow(const UiState& state);
  void drawRotationRow(const UiState& state);
  void drawWifiRow(const UiState& state);
  void drawFirmwareRow(const UiState& state);

  TFT_eSPI& tft_;
  WifiState lastWifi_ = WifiState::Idle;
  char lastSsid_[config::kMaxSsidLen + 1] = "";
  uint8_t lastRotate_ = 0;
  uint32_t lastCities_ = 0;
  uint32_t lastOta_ = 0;
};
