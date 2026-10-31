// Settings screen: language selector, touch calibration entry, WiFi status.
#pragma once

#include <TFT_eSPI.h>

#include "screen_main.h"
#include "theme.h"

enum class SettingsTarget : uint8_t {
  None,
  Back,
  Vietnamese,
  English,
  Calibrate,
};

class ScreenSettings {
 public:
  explicit ScreenSettings(TFT_eSPI& tft) : tft_(tft) {}

  // Full redraw.
  void show(const theme::Palette& palette, const UiState& state);
  // Redraws the WiFi row when the connection changes.
  void update(const UiState& state);

  SettingsTarget hitTest(int16_t x, int16_t y) const;

 private:
  void drawLanguageRow();
  void drawWifiRow(const UiState& state);

  TFT_eSPI& tft_;
  const theme::Palette* palette_ = &theme::defaultPalette();
  WifiState lastWifi_ = WifiState::Idle;
};
