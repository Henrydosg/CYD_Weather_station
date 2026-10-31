// City manager: the saved cities in rotation order, "+ Add", and a small
// action panel (Up / Down / Remove / Close) for the tapped city.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class CitiesTarget : uint8_t { None, Back, Add, MoveUp, MoveDown, Remove };

struct CitiesHit {
  CitiesTarget target = CitiesTarget::None;
  uint8_t index = 0;  // city for MoveUp / MoveDown / Remove
};

class ScreenCities {
 public:
  explicit ScreenCities(TFT_eSPI& tft) : tft_(tft) {}

  void show(const UiState& state);
  // Redraws when the city list changes.
  void update(const UiState& state);
  // Opening / closing the action panel is handled here.
  CitiesHit handleTap(int16_t x, int16_t y, const UiState& state);

 private:
  void drawList(const UiState& s);
  void drawPanel(const UiState& s);

  TFT_eSPI& tft_;
  int8_t selected_ = -1;  // city with the action panel open
  uint32_t lastCities_ = 0;
};
