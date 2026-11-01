// OTA update state shared with the UI as plain data (see ota_updater).
#pragma once

#include <stdint.h>

#include "config.h"

enum class OtaState : uint8_t {
  Idle,         // not checked yet
  Checking,     // manifest request runs on the next update()
  UpToDate,
  Available,    // OtaStatus::latest is newer than config::kFirmwareVersion
  Downloading,  // OtaStatus::progressPct
  Failed,       // check or install failed (retried at the next check)
  Restarting,
};

struct OtaStatus {
  OtaState state = OtaState::Idle;
  uint32_t version = 0;  // incremented on every change
  char latest[config::kMaxVersionLen + 1] = "";
  uint8_t progressPct = 0;
};
