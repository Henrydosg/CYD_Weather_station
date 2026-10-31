// Owns the screens, routes touch events to the active one and reports what
// the rest of the system must do (refresh, persist settings).
#pragma once

#include <TFT_eSPI.h>

#include "../settings/settings.h"
#include "screen_calibration.h"
#include "screen_main.h"
#include "screen_settings.h"
#include "touch_input.h"

enum class UiAction : uint8_t {
  None,
  RefreshWeather,
  LanguageChanged,   // see UiCommand::language
  CalibrationSaved,  // see UiCommand::calibration
};

struct UiCommand {
  UiAction action = UiAction::None;
  Language language = Language::Vietnamese;
  TouchCalibration calibration{};
};

class UiController {
 public:
  explicit UiController(TFT_eSPI& tft);

  // Sets up the display. Starts with calibration if it was never done.
  void begin(const Settings& settings);
  // Call every loop iteration.
  UiCommand update(const UiState& state, const TouchEvent& touch, uint32_t now);

 private:
  enum class Screen : uint8_t { Main, Settings, Calibration };

  void showMain();
  void showSettings(const UiState& state);
  void showCalibration(Screen returnTo);
  UiCommand changeLanguage(Language lang, const UiState& state);

  TFT_eSPI& tft_;
  ScreenMain main_;
  ScreenSettings settings_;
  ScreenCalibration calibration_;
  Screen screen_ = Screen::Main;
  Screen afterCalibration_ = Screen::Main;
};
