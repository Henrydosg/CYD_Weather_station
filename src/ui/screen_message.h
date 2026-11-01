// Remote message, full screen: envelope + "Message from <sender>", the
// word-wrapped text and an "OK" button. Opens over the main screen or the
// photo viewer when a new message is due; stays until the button is tapped.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

class ScreenMessage {
 public:
  explicit ScreenMessage(TFT_eSPI& tft) : tft_(tft) {}

  void show(const RemoteMessage& message);
  // True for a tap on (or near) the OK button.
  bool hitDone(int16_t x, int16_t y) const;

 private:
  TFT_eSPI& tft_;
};
