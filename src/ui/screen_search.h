// City search results (up to 5): name, region and population, with a "+"
// button to add the city. Shows "Searching..." / errors while waiting.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class SearchTarget : uint8_t { None, Back, Add };

struct SearchHit {
  SearchTarget target = SearchTarget::None;
  uint8_t index = 0;  // into UiState::searchResults
};

class ScreenSearch {
 public:
  explicit ScreenSearch(TFT_eSPI& tft) : tft_(tft) {}

  void show(const UiState& state, const char* query);
  void update(const UiState& state);
  SearchHit hitTest(int16_t x, int16_t y, const UiState& state) const;

 private:
  void drawBody(const UiState& s);
  bool canAdd(const UiState& s, uint8_t i) const;

  TFT_eSPI& tft_;
  char query_[config::kMaxSearchLen + 1] = "";
  uint32_t lastSearch_ = 0;
  uint32_t lastCities_ = 0;
};
