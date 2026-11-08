// Boot health: why the device (re)started, how often it crashed, and the
// rollback guard for firmware installed over WiFi.
//
// After an OTA install the new firmware boots "pending verify". It must show
// it works (WiFi up and one weather fetch) within config::kOtaVerifyWindowMs,
// then it is marked valid. Otherwise it marks itself invalid and the device
// restarts into the previous firmware. A reset before that (crash, watchdog,
// power cut) also returns to the previous firmware (done by the bootloader).
#pragma once

#include <stdint.h>

#include "config.h"

enum class ResetKind : uint8_t { PowerOn, Restart, Crash, Watchdog, Brownout, Other };

// Plain data for the UI.
struct BootReport {
  ResetKind reset = ResetKind::PowerOn;
  uint32_t boots = 0;    // since the counter was created (survives updates)
  uint32_t crashes = 0;  // resets by crash, watchdog or brownout
  bool verifying = false;  // new firmware not confirmed yet
  // Version that failed after an update and was rolled back ("" = none).
  char failedVersion[config::kMaxVersionLen + 1] = "";
};

class BootHealth {
 public:
  void begin();
  // healthy: WiFi connected and weather fetched at least once.
  void update(uint32_t now, bool healthy);
  const BootReport& report() const { return report_; }

 private:
  BootReport report_;
};
