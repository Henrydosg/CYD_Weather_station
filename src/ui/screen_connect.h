// Shown after the user picked a network: "Connecting...", then "Connected"
// (the controller returns to the main screen) or an error with
// "Try again" (password networks) and "Forget" (saved networks).
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class ConnectTarget : uint8_t { None, Back, Retry, Forget };

class ScreenConnect {
 public:
  explicit ScreenConnect(TFT_eSPI& tft) : tft_(tft) {}

  void start(const char* ssid, bool secured, bool saved);
  // Follows UiState::attempt; call every loop while shown.
  void update(const UiState& state, uint32_t now);
  ConnectTarget hitTest(int16_t x, int16_t y) const;
  // True once "Connected" has been shown long enough.
  bool finished(uint32_t now) const;
  const char* ssid() const { return ssid_; }

 private:
  enum class Phase : uint8_t { Waiting, Pending, Succeeded, Failed };

  void draw();
  bool hasRetry() const { return secured_; }
  bool hasForget() const { return saved_; }

  TFT_eSPI& tft_;
  char ssid_[config::kMaxSsidLen + 1] = "";
  bool secured_ = false;
  bool saved_ = false;
  // Waiting: the connect command has not reached the WiFi manager yet.
  Phase phase_ = Phase::Waiting;
  uint32_t doneAt_ = 0;
};
