// Firmware update screen (Settings -> Firmware): installed version, the state
// of the OTA check / download, one button (Check or Update) and a progress bar.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class UpdateTarget : uint8_t { None, Back, Action };

class ScreenUpdate {
 public:
  explicit ScreenUpdate(TFT_eSPI& tft) : tft_(tft) {}

  void show(const UiState& state);
  // Redraws when the OTA state or WiFi changes; only the bar while downloading.
  void update(const UiState& state);
  UpdateTarget hitTest(int16_t x, int16_t y, const UiState& state) const;

 private:
  void drawBody(const UiState& state);
  void drawProgress(uint8_t pct);
  static bool hasButton(const UiState& state);

  TFT_eSPI& tft_;
  uint32_t lastOta_ = 0;
  OtaState lastState_ = OtaState::Idle;
  bool lastOnline_ = false;
};
