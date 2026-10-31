// WiFi screen: status line, scanned networks (four per page, strongest
// first) with signal bars, lock, "saved" and "connected" marks.
#pragma once

#include <TFT_eSPI.h>

#include "screen_main.h"

enum class WifiTarget : uint8_t { None, Back, Rescan, PrevPage, NextPage, Network };

struct WifiHit {
  WifiTarget target = WifiTarget::None;
  uint8_t index = 0;  // into UiState::scanResults when target == Network
};

class ScreenWifi {
 public:
  explicit ScreenWifi(TFT_eSPI& tft) : tft_(tft) {}

  // Full redraw. resetPage: start again at the first page.
  void show(const UiState& state, bool resetPage);
  // Redraws the status line / list when their inputs change.
  void update(const UiState& state);
  // Page buttons are handled here; other targets go to the controller.
  WifiHit handleTap(int16_t x, int16_t y, const UiState& state);

 private:
  void drawStatus(const UiState& state);
  void drawList(const UiState& state);
  void drawRow(const WifiNetworkInfo& net, int16_t top, bool connected, bool saved);
  void drawPager(uint8_t pages);
  uint8_t pageCount(const UiState& state) const;
  void remember(const UiState& state);

  TFT_eSPI& tft_;
  uint8_t page_ = 0;
  // Inputs of the last redraw
  WifiState lastWifi_ = WifiState::Idle;
  char lastSsid_[config::kMaxSsidLen + 1] = "";
  uint32_t lastScanVersion_ = 0;
  uint8_t lastSavedCount_ = 0;
};
